[SUBSYSTEM::CLUSTER]
PRIVATE_DEPENDENCIES = TDB_WRAP

CLUSTER_OBJ_FILES = $(addprefix $(clustersrcdir)/, cluster.o local.o)
