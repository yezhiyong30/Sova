# Sova
Sova is a software-defined dynamic scheduling framework that combines the advantages of dynamic SR-IOV and VM live migration, and can greatly improve the network performance of the virtualized environment.

Sova works on top of popular Linux underlying techniques including ACPI Hotplug, Bonding Diver, etc, and also exploits often-used virtualization solutions, including para-virtual I/O, SR-IOV, live migration, etc. At the physical machine level, Sova can self-adjusting switch between the para-virtual NIC and the SR-IOV VF for each VM by sensing its workload, which can greatly improve the network performance of network-intensive VMs. On the other hand, at the cluster level, Sova dynamically adjusts the load for each physical machine through live migration technology, so it is prevented that the network performance is reduced due to the excessive load of a physical machine. In addition, Sova also coordinated the optimization method between the physical machine and the cluster by leveraging a software-defined way to make it work better.

Sova is developed based on Xen project (https://github.com/xen-project), and released with MIT license.

# Quick Start
Before running the program, you need to configure bonding for the NIC of each VM, and set each SR-IOV VF as the primary NIC of the bonding driver through active-backup way. If it is an HVM guest, you also need to configure a paravirtualized driver for xenstore data transfer (refer here: https://wiki.xen.org/wiki/Using_Xen_PV_Drivers_on_HVM_Guest). 

Xen needs to be recompiled, and replace the xl_vmcontrol.c file in the Dom0/scheduler folder and the xenbaked.c and xenmon.py files in the Dom0/xenmon folder.

The netinfo (in DomU folder, compile with libpthread and libxenstore libraries) runs as a daemon in domU, collects the network data of the VM and sends it to dom0 through xenstore. The scheduler (in Dom0/scheduler folder, compile with libraries including libxenlight, libxenstore, libxenctrl, libxentoollog and libm, etc) runs in dom0, and exploits the network data obtained from domU by xenstore and the I/O and cpu data obtained from XenMon to dynamically schedule the SR-IOV VF of the VM, and trigger and execute live migration. The controller (in Dom0/controller folder) runs on the central control node and manages the global information of the physical machines in the cluster.

# Contacts
This implementation is a research prototype that shows the feasibility. It is NOT production quality code. The technical details will be published in academic papers. If you have any questions, please raise issues on Github or contact the authors below.

zhiyong ye (yezhiyong30@163.com)

yang wang (yang.wang1@siat.ac.cn)

# Acknowledgments
This work is supported in part by National Key R\&D Program of China (2018YFB1004800) and in part by National Natural Science Foundation of China (61672513).
