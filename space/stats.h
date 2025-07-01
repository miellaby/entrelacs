#pragma once

/** statistic structure and variables
 */
struct s_space_stats {
    // operation and other events counters
    int get; //< GET op counter
    int root; //< root op counter
    int unroot; //< unroot op counter
    int new; //< new arrow event counter
    int found; //< found arrow event counter
    int connect; //< connect op counter
    int disconnect; //< disconnect op counter
    int atom; //< atom creation counter
    int pair; //< pair cration counter
    int forget; //< forget op counter
};

extern struct s_space_stats space_stats_zero, space_stats;

  