#!/bin/bash
# filepath: find_core_needs_qs_true.sh

awk '
BEGIN { RS="per_cpu\\(rcu_sched_data,"; ORS=""; }
/core_needs_qs = true/ && NR>1 {
    print "per_cpu(rcu_sched_data," $0 "\n----------------------------------------\n"
}
' p_rcu_sched_data_0-63.log | tee core_needs_qs_true.log