#!/bin/bash

awk '
BEGIN { RS="per_cpu\\(rcu_sched_data,"; ORS=""; }
/len *= *[1-9][0-9]*/ && NR>1 {
    print "per_cpu(rcu_sched_data," $0 "\n----------------------------------------\n"
}
' p_rcu_sched_data_0-63.log > rcu_sched_data_len_not_zero.log