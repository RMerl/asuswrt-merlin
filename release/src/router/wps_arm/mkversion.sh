#! /bin/bash
#
# Create the $2 file from $1 using the tag in $3 
#

# Check for the in file, if not there we're probably in the wrong directory
if [ ! -f $1 ]; then
	echo file "$1" not found
	exit 1
fi

	# Tag should be in the form
	#    <NAME>_REL_<MAJ>_<MINOR>
	# or
	#    <NAME>_REL_<MAJ>_<MINOR>_RC<RCNUM>
	# or
	#    <NAME>_REL_<MAJ>_<MINOR>_RC<RCNUM>_<INCREMENTAL>
	TAG=$3

	# Remove leading cvs "Name: " and trailing " $"
	TAG=${TAG/#*: /}
	TAG=${TAG/% $/}

	# Split the tag into an array on underbar or whitespace boundaries.
	IFS="_	     " tag=(${TAG})
	unset IFS

        tagged=1
	if [ ${#tag[*]} -eq 0 ]; then
	   tag=(`date '+TOT REL %Y %m %d 0 %y'`);
	   tagged=0
	fi

	# Allow environment variable to override values.
	# Missing values default to 0
	#
	maj=${MOD_MAJOR_VERSION:-${tag[2]:-0}}
	min=${MOD_MINOR_VERSION:-${tag[3]:-0}}
	rcnum=${MOD_RC_NUMBER:-${tag[4]:-0}}
	incremental=${MOD_INCREMENTAL_NUMBER:-${tag[5]:-0}}
	build=${MOD_BUILD_NUMBER:-0}

	# Strip 'RC' from front of rcnum if present
	rcnum=${rcnum/#RC/}
	
	# strip leading zero off the number (otherwise they look like octal)
	maj=${maj/#0/}
	min=${min/#0/}
	min_router=${min}
	rcnum=${rcnum/#0/}
	incremental=${incremental/#0/}
	build=${build/#0/}

	# some numbers may now be null.  replace with with zero.
	maj=${maj:-0}
	min=${min:-0}
	rcnum=${rcnum:-0}
	incremental=${incremental:-0}
	build=${build:-0}

	if [ ${tagged} -eq 1 ]; then
	    vernum=`printf "0x%02x%02x%02x%02x" ${maj} ${min} ${rcnum} ${incremental}`
	else 
	    vernum=`printf "0x00%02x%02x%02x" ${tag[7]} ${min} ${rcnum}`
	fi


        # PR17029: increment minor number for tagged router builds
        #         with an even minor revision
	if [ ${tagged} -eq 1 -a `expr \( \( ${min} + 1 \) % 2 \)` -eq 1 ]; then
	   min_router=`expr ${min} + 1`
	fi


	# OK, go do it

	echo "maj=${maj}, min=${min}, rc=${rcnum}, inc=${incremental}, build=${build}"
	echo "Router maj=${maj}, min=${min_router}, rc=${rcnum}, inc=${incremental}, build=${build}"
	
	sed \
		-e "s;@MOD_MAJOR_VERSION@;${maj};" \
		-e "s;@MOD_MINOR_VERSION@;${min};" \
		-e "s;@MOD_RC_NUMBER@;${rcnum};" \
		-e "s;@MOD_INCREMENTAL_NUMBER@;${incremental};" \
		-e "s;@MOD_BUILD_NUMBER@;${build};" \
		-e "s;@MOD_VERSION@;${maj}, ${min}, ${rcnum}, ${incremental};" \
		-e "s;@MOD_VERSION_STR@;${maj}.${min}.${rcnum}.${incremental};" \
                -e "s;@MOD_ROUTER_VERSION_STR@;${maj}.${min_router}.${rcnum}.${incremental};" \
                -e "s;@MOD_VERSION_NUM@;${vernum};" \
		< $1 > $2
