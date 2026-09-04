# Entrelacs : vers un scheduler coopératif réactif

Synthèse d'une discussion sur l'évolution du système Entrelacs vers un modèle
d'agents persistants et réactifs, tout en respectant l'API sérialisée actuelle.

## Contexte de départ

Le projet Entrelacs est un protocole expérimental de calcul basé sur les
flèches (arrow-based computing). L'API est sérialisée (mutex global `apiMutex`
dans `space.c:27`), la machine a de l'état global mutable (`chainSize` marqué
`TODO thread safe` en `machine.c:248`, pool transitoire global en
`transient.c:58`). Le serveur HTTP (`server.c`) est multithread via Mongoose :
un worker par requête, chacun appelant `xs_eval` de façon synchrone et
bloquante.

`doc/PLANS.txt` liste déjà plusieurs pistes (message bus, pause/resume,
box/observe/trigger, HTTP push, session-as-continuation) mais aucune n'est
bâtie.

## La direction

Faire d'Entrelacs une plateforme d'agents persistants et réactifs, où l'état
de calcul vit dans le graphe des flèches, pas dans des threads bloqués. Le
principe fondateur — toute donnée est une flèche dédupliquée persistante —
s'étend à l'état de calcul lui-même.

## Les contraintes identifiées

1. **Pas de scheduler existant.** L'exécution est synchrone : `xs_eval` appelle
   `xs_run` qui boucle jusqu'à un état terminal, puis répond la requête HTTP.
   La boucle `houseCleaning` (`server.c:417`) ne schedule rien.

2. **Pas de `xs_run` parallèle.** `machine.c:248` a `static int chainSize = 0;
   // TODO thread safe`. Le pool transitoire est un `static struct` global
   (`transient.c:58`). Deux `xs_run` sur deux threads écriraient dans le même
   état. Linux peut scheduler les threads, mais le code casserait.

3. **Conclusion : le scheduler n'est pas Linux.** C'est une boucle coopérative
   monothread. Un seul `xs_run` à la fois. La concurrence ne se manifeste qu'au
   niveau du graphe (workers qui écrivent des flèches), pas au niveau de
   l'exécution de la machine.

## Séparation de `xs_run`

`xs_run` (`machine.c:982`) fait aujourd'hui trois choses en une seule
fonction :

| Responsabilité | Code actuel | Problème pour un scheduler |
|---|---|---|
| Initialiser la machine | `machine_init(session)` | À faire une fois, pas par continuation |
| Boucler sur `transition` | le `while` | C'est ça, le « run » |
| Construire l'état initial `M` | fait par `xs_eval` avant l'appel | Pas réutilisable pour reprendre un état |

### Séparation proposée

```
xs_run(C, M, session)          → aujourd'hui: init + boucle + retour valeur
                                    ↓ séparer en ↓
machine_setup(session)         → machine_init (opérateurs, mots-clés) — une fois
xs_step(C, M) → (C', M')       → UNE transition — l'unité atomique du scheduler
xs_run(C, M, session)          → la boucle while, maintenant triviale
xs_eval(C, p, session)         → construit M initial, appelle xs_run — inchangé
```

Le point clé : `xs_step` prend un état `M` et retourne le suivant.
`transition` (`machine.c:250`) fait déjà exactement ça, sauf qu'elle est
`static` et retourne un `(C, M)` combiné. Il suffirait de l'exposer.

`xs_run` devient un cas particulier : appeler `xs_step` en boucle jusqu'au
terminal, sans jamais s'arrêter. C'est le comportement synchrone d'aujourd'hui,
inchangé. Le scheduler coopératif est la même boucle, mais qui interrompt quand
`transition` produit un état « en attente ».

## La boucle unique (le scheduler)

Une seule boucle, toutes les continuations, un thread. Au lieu de N workers
chacun avec son `xs_run` bloquant :

```
worker 1: xs_eval → xs_run (bloqué 2s)
worker 2: xs_eval → xs_run (bloqué 3s)
worker 3: xs_eval → xs_run (bloqué 0.5s)
```

Demain, un seul consommateur :

```
workers: déposent continuations, répondent 202, disparaissent
       ↓
boucle unique (main thread):
    pour chaque continuation dans /ready:
        (C, M) = xs_step(C, M)   × budget
        si terminée   → résultat
        si yield      → /waiting
        sinon         → remet dans /ready
```

### Ce que ça élimine mécaniquement

- Le `TODO thread safe` sur `chainSize` disparaît par construction : un seul
  thread touche la machine.
- Les workers ne font que des opérations `xl_*` sur le graphe, déjà protégées
  par `apiMutex`. Le problème de réentrance de la machine ne se pose plus.
- `chainSize < 500` devient un *budget par tour*, pas une limite globale. Une
  continuation longue ne bloque plus personne : elle consomme son budget,
  retourne dans `/ready`, et le tour suivant reprend.

### La question du budget

Par tour : `xs_step × N` sur continuation A, puis B, etc. Trop petit →
beaucoup d'aller-retours dans le graphe (root/unroot à chaque tour). Trop grand
→ une continuation affame les autres. C'est un niveau de fair-scheduling
réglable, pas un problème d'architecture. Le budget ne sert qu'à protéger
contre les programmes qui ne yield jamais volontairement.

## La file est le graphe

La « file » n'est pas une structure de données à inventer. C'est une vue sur
le graphe : les continuations *sont* les flèches rootées sous un tag donné.

```
/ready+M      continuation prête à exécuter
/waiting+M    continuation en attente d'un événement
```

Déposer dans la file se réduit à `xs_root(xs_pair(readyTag, M))`. Piocher
revient à parcourir `xl_childrenOf(readyTag)`. Le pattern existe déjà dans le
code : le housecleaning (`server.c:311`) fait exactement ça pour les sessions
(parcourt `xl_childrenOf(sessionTag)`, filtre les rootées, nettoie les
expirées).

`trigger` ne fait que déplacer une flèche d'un tag à l'autre :
`unroot(/waiting+M)` puis `root(/ready+M)`.

Le seul état C hors graphe légitime est un mutex + condvar pour protéger la
*modification* de la file contre deux workers simultanés. Mais `apiMutex` de
`space.c` sérialise déjà chaque `xl_root`/`xl_unroot` individuellement.

## Pause / resume explicite

`pause` ne bloque pas. Il sérialise l'état machine (`M = /p/e+k`) comme une
flèche rootée dans le graphe, puis *retourne normalement*. Le thread HTTP
répond, libère sa transaction, et disparaît. La continuation existe
uniquement comme une flèche rootée — pas de pile C, pas de thread, pas de
transaction ouverte.

L'analogie n'est pas une attente bloquante (comme un OS non-préemptif où la
tâche garde sa pile et son slot). C'est une *absence de scheduling* : la
continuation en attente n'est pas dans l'ensemble des continuations prêtes.
Quand `trigger` arrive, il déplace la continuation de « en attente » à « prête
» et le scheduler la reprend au passage suivant.

### Le flux HTTP

```
client POST /program
  ↓
scheduler: xs_step jusqu'à yield
  ↓
yield → continuation M persistée (rootée sous son digest)
  ↓
HTTP 202 + Location: /continuation/<digest>
  ↓
... plus tard ...
  ↓
client GET /continuation/<digest>  ← c'est le resume
  ↓
scheduler: xs_step jusqu'à yield suivant ou terminaison
  ↓
soit 200 + résultat   (terminaison)
soit 202 + nouveau Location   (yield à nouveau)
```

`done` n'existe pas comme file séparée. La continuation yielded *est* la
chose stockée, et elle porte son propre handle (son URI). Le resume est
piloté par le client, pas par une queue à vider.

La continuation `M` est une flèche. Elle a un digest (`xs_getDigest`, déjà
dans le code). Le digest *est* l'URI. Donc `/continuation/<digest>` se résout
directement par `xs_digestMaybe` — l'infrastructure d'adressage par digest
existe déjà.

### Nettoyage des continuations abandonnées

Les continuations que le client ne rappelle jamais sont nettoyées par le
housecleaning existant, avec le même pattern que les sessions expirées :
parcourir les enfants rootés d'un tag, lire un champ `expire`, unroot ce qui
a dépassé. Il suffit d'appliquer le pattern à `/continuation` en plus de
`/session`. Zéro mécanisme nouveau.

## Réponse HTTP : deferred sync

Le thread de la réponse HTTP ne lance pas `xs_run` — il dépose et attend un
résultat. La boucle unique fait le travail. Les deux communiquent par le
graphe plus un point de synchronisation minimal.

```
worker:                               boucle unique (main):
  déposer M dans /ready                 piocher M dans /ready
  attendre /done+<digest>               xs_step × budget
    ou /waiting+<digest>                si termine → root /done+<digest>+résultat, signal
    ou timeout 10s                      si yield → root /waiting+<digest>, signal
                                        sinon → remettre /ready
  au réveil:
    /done trouvé   → 200 + résultat, unroot /done
    /waiting trouvé → 202 + Location
    timeout        → 202 + Location (encore en /ready)
```

Le worker ne touche jamais la machine. Il ne fait que des `xl_*` sur le
graphe (déjà protégées par `apiMutex`) et un `pthread_cond_wait` avec timeout.
La boucle unique reste le seul exécuteur de `xs_step`.

Un seul `pthread_cond_t` partagé. La boucle signale après chaque continuation
traitée (terminée ou yielded). Les workers en attente se réveillent,
vérifient si leur digest est dans `/done` ou `/waiting` via le graphe,
répondent ou se rendorment.

### Ce que ça achète

Le cas courant — une évaluation qui termine en 200ms — reste synchrone côté
client : une seule requête, réponse immédiate, pas de polling. Le cas long —
yield ou calcul qui dépasse 10s — bascule en asynchrone sans intervention.
C'est le *deferred sync* pattern : 200 si rapide, 202 si lent, transparent
pour le client simple.

## Ordre de construction

1. **`xs_step`** : exposer `transition` comme API publique. C'est la condition
   préalable à tout le reste. Pas de refactor destructif : `xs_run` devient
   juste une boucle sur `xs_step`.
2. **Boucle unique dans `server.c`** : le `main` (`server.c:417`) arrête de
   `sleep(1)` et devient le scheduler. Les workers ne font que déposer des
   continuations et attendre un résultat.
3. **`yield`/`pause`** : une transition qui produit un état « en attente » au
   lieu d'un nouvel état à exécuter immédiatement. La continuation est rootée
   sous son digest, le worker reçoit 202 + Location.
4. **`resume` via URI** : le client rappelle l'URI de la continuation, le
   scheduler la remet dans `/ready` et la reprend.
5. **Housecleaning des continuations abandonnées** : réutiliser le pattern
   existant (`server.c:311`) appliqué à `/continuation`.
6. **(Plus tard) `box`/`observe`/`trigger`** : le resume automatique comme
   raccourci qui appelle le resume pour toi. C'est la couche réactive du
   PLANS.txt, bâtie par-dessus le resume explicite.

Chaque pièce se construit sur la précédente sans refactor destructif. Le
test de validation minimal à l'étape 3 : `pause`, `kill -9`, relancer, `resume`
donne le même résultat — une continuation qui survit à un crash parce qu'elle
n'a jamais quitté le graphe.
