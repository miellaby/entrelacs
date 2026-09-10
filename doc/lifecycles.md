# Cycles de vie

Trois cycles de vie se superposent dans Entrelacs :

1. **le pool transitoire** — combien de temps un pointeur `Arrow` reste valide ;
2. **la transaction** — quand les threads (et les processus) se synchronisent ;
3. **l'enracinement** — ce qui survit au ramasse-miettes.

Chacun est simple pris séparément. Ce sont leurs *intersections* qui sont
piégeuses, et c'est là que se logent les bugs de durée de vie. Ce document
décrit les trois axes puis leurs points de croisement.

À lire avant de toucher à `machine/`, `server/` ou `machine/session.c`.

## Vue d'ensemble

| Axe | Unité | Créé par | Invalidé par | Protégé par |
|---|---|---|---|---|
| Pool transitoire | pointeur `Arrow` | `xs_pair`, `xs_atom`, `xs_const`, `xs_uri`… | `xs_pool_reset()` — **en bloc** | rien : il faut retenir une `Address` |
| Transaction | `transactionCount` | `xl_open()` | `xl_close()` / `xl_commit()` | mutex récursif `apiMutex` |
| Enracinement | flèche dans l'espace | `xs_assimilate` / `xs_root` | `weaver_performGC()` au rendez-vous | racine (`xl_root`) ou ancêtre rooté |

## Axe 1 — Le pool transitoire

Un `Arrow` de l'API `xs_` **est un pointeur** dans un pool global de chunks
(`machine/transient.c:58`, allocation par `arrow_new`, `transient.c:78`). Le
pool croît géométriquement et n'est jamais libéré à la pièce.

`xs_pool_reset()` (`transient.c:167`) le vide **entièrement** : il appelle les
destructeurs de hooks et libère les buffers d'atomes non empruntés
(`arrow_free`, `transient.c:154`), puis rend tous les chunks sauf le premier
(`pool_reset`, `transient.c:112`). Après cet appel, **tout** pointeur `Arrow`
détenu par qui que ce soit est pendant.

Qui appelle `xs_pool_reset()` :

- `xs_session_commit()` (`machine/session.c:67`) ;
- `xs_session_close()` (`machine/session.c:79`) ;
- donc l'opérateur `commit` **depuis l'intérieur de la machine**, via
  `_machine_commit` (`machine/machine.c:68`) ;
- la boucle du REPL entre deux commandes (`repl/repl.c:288`).

Survivent au reset, parce qu'ils sont hors du pool : `xs_eve()`
(`transient.c:41-42`) et `xs_atom_session()` (`transient.c:583`), deux
`ArrowValue` statiques.

### Conséquences

1. **Ce qui doit traverser un reset est une `Address`, pas un pointeur.**
   L'idiome est : assimiler, lire l'`Address` avec `xs_getId()`, puis
   réhydrater un pointeur avec `xs_arrow(id)` après le reset.
2. **`machine.c` met en cache une vingtaine de flèches-mots-clés dans des
   globales** (`machine.c:22-25`). Elles pointent dans le pool. Tout reset
   impose donc un `update_keywords()` (`machine.c:28`) avant la moindre
   comparaison — c'est ce que fait `_machine_commit`.
3. **`arrow_new()` ne prend aucun verrou.** Le pool n'est pas thread-safe,
   alors que le serveur HTTP appelle l'API `xs_` depuis chaque worker
   (`server/server.c:186-221` : `xs_url`, `xs_context_set`, `xs_atom`…). C'est
   un défaut latent aujourd'hui, et un bloqueur dur pour tout scheduler : un
   worker qui commite vide le pool sous les pieds de la machine.

> **Règle.** Un `Arrow` est un pointeur local à une phase d'exécution, entre
> deux resets. Il ne se stocke pas, ne se partage pas entre threads, ne
> traverse pas un commit.

## Axe 2 — La transaction

`xl_open()` (`space/space.c:759`) incrémente `transactionCount` et ouvre la
mémoire. `xl_close()` (`space.c:766`) et `xl_commit()` (`space.c:801`) sont un
**rendez-vous** : le thread quitte sa transaction puis attend
(`WAIT_DORMANCY()`, `space.c:45`) que `transactionCount` retombe à zéro, donc
que *tous* les autres threads soient sortis de la leur. Alors seulement il
exécute `weaver_performGC()`, `mem_commit()`, et pour `xl_close()`
`mem_close()`.

Trois conséquences, dans l'ordre d'importance :

1. **Le GC ne tourne jamais tant qu'une transaction est ouverte.** C'est la
   protection réelle des flèches de travail (voir axe 3). Ce n'est pas une
   propriété accessoire du rendez-vous : c'est son rôle.
2. **On n'attend jamais en gardant une transaction ouverte.** Un
   `pthread_cond_wait`, une lecture socket, un `sleep` à l'intérieur d'une
   transaction gèle le commit de tous les autres threads. Symétriquement, une
   boucle qui vit dans une transaction ouverte permanente empêche tout commit
   dans le processus.
3. **`apiMutex` rend chaque appel `xl_*` atomique, pas les séquences.**
   Le verrou est pris et rendu par appel (`LOCK()`/`LOCK_OUT()`,
   `space.c:29-34`). Deux threads s'entrelacent librement entre deux appels ;
   un « lire puis écrire » n'est pas protégé.

Il y a un quatrième niveau, inter-processus : `mem_open()` → `mem0_open()`
prend un `flock(LOCK_EX)` **bloquant** sur le fichier de persistance
(`mem/mem0.c:284`), et si le fichier a changé entre-temps, tout le cache RAM
est invalidé (`mem/mem.c:157-164`). Fermer sa transaction laisse donc entrer
un autre processus `entrelacs`, et la rouvrir peut coûter la totalité du cache.

> **Règle.** La transaction est à la fois l'unité d'atomicité et la fenêtre de
> protection contre le GC. Aussi courte que possible — mais jamais en attente
> bloquante.

## Axe 3 — L'enracinement

`xs_assimilate()` (`transient.c:455`) projette une flèche transiente dans
l'espace et lui donne une `Address`. Une flèche nouvellement créée est
**immédiatement inscrite au journal des flèches lâches** (`weaver_addLoose`,
appelé en `space/assimilate.c:133`, `:417`, `:560`).

Est *lâche* une flèche ni rootée ni référencée par un enfant
(`cell_isLoose`, `space/cell.c:391`). `weaver_performGC()`
(`space/weaver.c:418`) parcourt le journal et oublie définitivement tout ce
qui est encore lâche au moment du passage. `xl_root()` (`space.c:667`) sort la
flèche du journal ; `xl_unroot()` (`space.c:706`) l'y remet si elle n'a pas
d'enfant.

D'où le point le plus contre-intuitif du système :

> **Une flèche assimilée mais non rootée vit exactement jusqu'au prochain
> commit — celui de n'importe quel thread.** Elle n'est protégée que parce que
> votre transaction est ouverte (axe 2, point 1).

`xs_root()` (`transient.c:541`) assimile puis roote : c'est le seul moyen de
faire traverser un commit à une flèche, directement ou par un ancêtre rooté.

## L'intersection : `_machine_commit`

Les trois axes se croisent dans six lignes, `machine/machine.c:64-74`. C'est
l'idiome de référence, à copier plutôt qu'à réinventer :

```c
xs_root(xs_pair(selfM, CM));       // axe 3 : ancrer l'état avant le GC
Address CM_id = xs_getId(CM);      // axe 1 : retenir l'Address, le pointeur va mourir
session = xs_session_commit(session); // axes 2+1 : rendez-vous, GC, mem_commit, pool_reset
update_keywords();                 // axe 1 : recréer les mots-clés cachés en globales
CM = xs_arrow(CM_id);              // axe 1 : réhydrater un pointeur depuis l'Address
xs_unroot(xs_pair(selfM, CM));     // axe 3 : relâcher l'ancre
```

L'ordre n'est pas négociable : rooter **avant** le commit, lire l'`Address`
**après** l'assimilation (que `xs_root` a faite), `update_keywords()` **avant**
tout code qui dépend d'un `xs_const`, réhydrater **avant** de déraciner —
sinon le `xs_pair(selfM, CM)` du dernier appel désigne une autre flèche.

## Le nœud non résolu : `xl_yield`

`PLANS.txt` annonce `xl_yield(state)` : « sync with all threads then GC, but in
addition: preserve "state" arrow from GC », et « is used between each
transition of the machine ». Le corps est désactivé (`space/space.c:835`) :

> `return; // FIXME je désactive yield tant que je n'ai pas trouvé une façon`
> `correcte de préserver des flèches manipulées en dehors de xs_run()`

Relu à travers les trois axes, le problème est clair : au rendez-vous, ce sont
les flèches lâches de **tous** les participants qui meurent. Préserver *une*
flèche d'état suffit à la machine, mais pas au worker HTTP qui tient au même
instant une URL analysée, une variable de session, un corps de requête — les
« flèches manipulées en dehors de `xs_run()` ».

Deux formes de solution, à trancher avant de bâtir dessus :

- **(a) un jeu de racines par participant** (thread ou continuation), rooté
  temporairement le temps de la barrière puis relâché ;
- **(b) interdire de franchir une barrière avec des flèches lâches** : chaque
  participant roote ou abandonne avant. C'est ce que `_machine_commit` fait à
  la main, pour un seul état.

## Conséquences pour un scheduler coopératif

Les invariants qu'un ordonnanceur de continuations doit respecter, déduits des
trois axes :

1. **Un seul thread exécute la machine.** Les globales de `machine.c:22-25`,
   `chainSize` (`machine.c:248`) et le pool transitoire sont partagés et non
   protégés. Ce n'est pas Linux qui ordonnance, c'est une boucle coopérative.
2. **Rendre le pool transitoire propre à un thread avant toute boucle
   unique** — sinon un worker qui commite vide le pool de la machine. Les
   `Arrow` étant des pointeurs, ils n'ont de toute façon pas vocation à
   traverser un thread : le passage se fait par `Address`, à travers le graphe.
3. **Une continuation en attente doit être rootée**, et adressée par
   `Address` (ou par le digest d'une flèche rootée), jamais par pointeur.
4. **Aucun thread n'attend un résultat en gardant une transaction ouverte**
   (axe 2, point 2).
5. **`chainSize` est une profondeur de pile de continuations**, incrémentée à
   l'empilement (`machine.c:280`, `:319`, `:421`…) et décrémentée au
   dépilement (`machine.c:483`, `:503`, `:506`). La limite de 500 protège
   contre la récursion infinie. Un budget de pas par tour est un compteur
   *distinct*, à ajouter.
6. **Un état contenant un hook n'est pas persistable.** `xs_operator` et
   `xs_continuation` (`machine.c:58`, `:62`) encapsulent un pointeur C via
   `xs_hook` (`transient.c:567`), et `e` comme `k` peuvent en contenir. Les
   opérateurs système sont nommables par index dans la table `systemFns`
   (`machine.c:921-943`) ; les hooks dynamiques, tel l'`XLEnum` de
   `childrenEnumHook` (`machine.c:710-727`), ne le sont pas — un `pause` sur un
   tel état doit échouer explicitement.
7. **La file d'attente est une vue sur le graphe** : les continuations prêtes
   sont les flèches rootées sous un tag. Le balayage existe déjà,
   `_houseCleaning` (`server/server.c:311`) : enfants rootés d'un tag, champ
   `expire`, déracinement des expirés.
