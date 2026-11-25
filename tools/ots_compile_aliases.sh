#!/bin/bash
# Source this to get color code variables to use during output

# Spack 1.1 (the artdaq/Spack1.1 spack-mpd) has no 'spack mpd build -G <generator>': the generator is chosen once when the
# project is created (generator=ninja). Older Spack (0.28 area) still needs '-G Ninja'. The version is the first word of
# 'spack --version', e.g. "1.1.0 (<commit>)" versus "1.0.0.dev0 (<commit>)".
ots_spack_is_1_1() {
	[[ "$(spack --version 2>/dev/null)" == 1.1.* ]]
}
ots_mpd_generator_option() {
	if ots_spack_is_1_1; then echo ""; else echo "-G Ninja"; fi
}

# setup compile aliases (for more spack debug info, use spack -d install -j$CETPKG_J 2>&1)
#============================
lockfile="/tmp/ots_compile_${OTS_SOURCE//\//_}.lock"

get_lock() {
	# Attempt to create the lock file atomically using ln
	if ! ln -s "$$" "$lockfile" 2>/dev/null; then
	# Check if the existing lock file contains a valid PID
	if [ -L "$lockfile" ]; then
			pid=$(readlink "$lockfile")
			if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "Script is already running with PID $pid."
		return 1
			fi
	fi
	# If stale, remove it and try again
	rm -f "$lockfile"
	if ! ln -s "$$" "$lockfile" 2>/dev/null; then
			echo "Failed to acquire lock."
			return 1
	fi
	fi
	# Ensure lock file is removed on exit
	return 0
}

rel_lock() {
	[ -n "$lockfile" ] || return 0
	rm -f -- "$lockfile"
}

Base=$SCRIPT_DIR
escaped_srcs=$(printf '%s\n' "$OTS_SOURCE/" | sed 's/[\/&]/\\&/g')
#  alias  mb='date; start_time=$(date +%s); spack find | grep gcc; spack mpd build -G Ninja -j$CETPKG_J 2>&1 | sed s/$escaped_srcs//g | sed s/__spack_path_placeholder__//g | sed s/\\\[padded-to-255-chars\\\]//g | sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g; pushd $Base/build; ninja install | sed s/$escaped_srcs//g; popd; end_time=$(date +%s); date; delta_time=$((end_time - start_time)); fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc); echo "Full time: $delta_time seconds or $fractional_minutes minutes"'
unalias mb 2>/dev/null
mb() {
	date;
	get_lock && {
	trap 'rm -f "$lockfile"; echo removing lockfile' RETURN
	trap 'rm -f "$lockfile"; echo SIGINT; return 0' SIGINT # may need to be 'exit 0'???
	start_time=$(date +%s);
	# spack find | grep gcc;

	# Temporary file to store processed build output
	temp_file=$(mktemp)

	# Run spack mpd build with tee to process output via sed, display, and save to a temp file
	stdbuf -oL spack mpd build $(ots_mpd_generator_option) -j$CETPKG_J 2>&1 | \
		tee >(sed s/$escaped_srcs//g | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/required\ from\ here/\\n\\nrequired\ from\ here\ \<=============\\n\\n/g | \
			   sed s/note\:\ in\ expansion\ of\ /note\:\ in\ expansion\ of\ \<=============\\n/g | \
			   sed s/undefined\ reference/\\n\\nundefined\ reference\ \<=============\ /g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g | \
			   tee "$temp_file")

	# Read temp file into variable for further checks
	build_output=$(cat "$temp_file")
	rm "$temp_file"  # Clean up the temporary file

	# Check if the build failed based on specific error message
	if echo "$build_output" | grep -q "ninja: build stopped: subcommand failed."; then
		echo "Build failed! Skipping ninja install."
	else
		# Only run install if build succeeded
		echo "install at $Base/build"
		pushd $Base/build;
		ninja install | sed s/$escaped_srcs//g | \
			   grep -v "Up-to-date:" | \
			   grep -v "\/\.\/README\.md" | \
			   grep -v "\/\.\/LICENSE" | \
			   grep -v "Set\ non-toolchain\ portion\ of" | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/\ to\ \".*\"//g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g;
		popd;

		#only run warn check if build succeeded
		if [ $CHECK_GIT_REPO_STATUS == 1 ]; then
			echo -e "$(date +%d%h%y.%T) setup_ots.sh:${LINENO} |  \t Will report a warning if any uncommitted code found..."
			UpdateOTS.sh --warnfast 2>&1 >/dev/null
			echo #to get back to terminal
		fi
	fi


	end_time=$(date +%s);
	date;
	delta_time=$((end_time - start_time));
	fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc);
	echo "Full time: $delta_time seconds or $fractional_minutes minutes";
	rel_lock;
	trap - RETURN
	trap - SIGINT
	} || { echo status=$?;
	echo -e "$(date +%d%h%y.%T) setup_ots.sh:${LINENO} | \t ERROR! Another user appears to be compiling. Only one user is allowed to compile at a time in each source area.."; }
} #end mb

#  alias  ml='date; start_time=$(date +%s); spack find | grep gcc; spack mpd build -G Ninja -j$CETPKG_J 2>&1 | sed s/$escaped_srcs//g | sed s/__spack_path_placeholder__//g | sed s/\\\[padded-to-255-chars\\\]//g | sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g | tee m.txt; pushd $Base/build; ninja install | sed s/$escaped_srcs//g; popd; end_time=$(date +%s); date; delta_time=$((end_time - start_time)); fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc); echo "Full time: $delta_time seconds or $fractional_minutes minutes"; less m.txt'
unalias ml 2>/dev/null
ml() {
	date;
	get_lock && {
	trap 'rm -f $lockfile; echo removing lockfile' RETURN
	trap 'rm -f $lockfile; echo SIGINT; return 0' SIGINT # may need to be 'exit 0'???
	start_time=$(date +%s);
	# spack find | grep gcc;

	# Temporary file to store processed build output
	temp_file=".ml_log.txt"

	# Run spack mpd build with tee to process output via sed, display, and save to a temp file
	stdbuf -oL spack mpd build $(ots_mpd_generator_option) -j$CETPKG_J 2>&1 | \
		tee >(sed s/$escaped_srcs//g | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/required\ from\ here/\\n\\nrequired\ from\ here\ \<=============\\n\\n/g | \
			   sed s/undefined\ reference/\\n\\nundefined\ reference\ \<=============\ /g | \
			   sed s/note\:\ in\ expansion\ of\ /note\:\ in\ expansion\ of\ \<=============\\n/g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g | \
			   tee "$temp_file")

	# Read temp file into variable for further checks
	build_output=$(cat "$temp_file")
	# rm "$temp_file"  # Clean up the temporary file

	# Check if the build failed based on specific error message
	if echo "$build_output" | grep -q "ninja: build stopped: subcommand failed."; then
		echo "Build failed! Skipping ninja install."
	else
		# Only run install if build succeeded
		pushd $Base/build;
		ninja install | sed s/$escaped_srcs//g | \
			   grep -v "Up-to-date:" | \
			   grep -v "\/\.\/README\.md" | \
			   grep -v "\/\.\/LICENSE" | \
			   grep -v "Set\ non-toolchain\ portion\ of" | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/\ to\ \".*\"//g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g;
		popd;
	fi

	end_time=$(date +%s);
	date;
	delta_time=$((end_time - start_time));
	fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc);
	echo "Full time: $delta_time seconds or $fractional_minutes minutes"
	less $temp_file
	rm "$temp_file"  # Clean up the temporary file
	rel_lock;
	trap - RETURN
	trap - SIGINT
	} || { echo status=$?;
	echo -e "$(date +%d%h%y.%T) setup_ots.sh:${LINENO} | \t ERROR! Another user appears to be compiling. Only one user is allowed to compile at a time in each source area.."; }
} #end ml

# alias  mz='date; start_time=$(date +%s); spack find | grep gcc; spack mpd build -G Ninja --clean -j$CETPKG_J 2>&1 | sed s/$escaped_srcs//g | sed s/__spack_path_placeholder__//g; pushd $Base/build; ninja install | sed s/$escaped_srcs//g; popd; end_time=$(date +%s); date; delta_time=$((end_time - start_time)); fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc); echo "Full time: $delta_time seconds or $fractional_minutes minutes"'
unalias mz 2>/dev/null
mz() {
	date;
	get_lock && {
	trap 'rm -f $lockfile; echo removing lockfile' RETURN
	trap 'rm -f $lockfile; echo SIGINT; return 0' SIGINT # may need to be 'exit 0'???
	start_time=$(date +%s);
	rm -rf $SCRIPT_DIR/local/install
	# spack find | grep gcc;

	# Temporary file to store processed build output
	temp_file=$(mktemp)

	# Run spack mpd build with tee to process output via sed, display, and save to a temp file
	stdbuf -oL spack mpd build $(ots_mpd_generator_option) --clean -j$CETPKG_J 2>&1 | \
		tee >(sed s/$escaped_srcs//g | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/required\ from\ here/\\n\\nrequired\ from\ here\ \<=============\\n\\n/g | \
			   sed s/undefined\ reference/\\n\\nundefined\ reference\ \<=============\ /g | \
			   sed s/note\:\ in\ expansion\ of\ /note\:\ in\ expansion\ of\ \<=============\\n/g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g | \
			   tee "$temp_file")

	# Read temp file into variable for further checks
	build_output=$(cat "$temp_file")
	rm "$temp_file"  # Clean up the temporary file

	# Check if the build failed based on specific error message
	if echo "$build_output" | grep -q "ninja: build stopped: subcommand failed."; then
		echo "Build failed! Skipping ninja install."
	else
		# Only run install if build succeeded
		pushd $Base/build;
		ninja install | sed s/$escaped_srcs//g | \
			   grep -v "Up-to-date:" | \
			   grep -v "\/\.\/README\.md" | \
			   grep -v "\/\.\/LICENSE" | \
			   grep -v "Set\ non-toolchain\ portion\ of" | \
			   sed s/__spack_path_placeholder__//g | \
			   sed s/\\\[padded-to-255-chars\\\]//g | \
			   sed s/\ to\ \".*\"//g | \
			   sed s/\\\/tdaq-v......../\\\/tdaq-v_\ \ \ /g;
		popd;
	fi

	end_time=$(date +%s);
	date;
	delta_time=$((end_time - start_time));
	fractional_minutes=$(echo "scale=1; $delta_time / 60" | bc);
	echo "Full time: $delta_time seconds or $fractional_minutes minutes"
	rel_lock;
	trap - RETURN
	trap - SIGINT
	} || { echo status=$?;
	echo -e "$(date +%d%h%y.%T) setup_ots.sh:${LINENO} | \t ERROR! Another user appears to be compiling. Only one user is allowed to compile at a time in each source area.."; }
} #end mz
alias mz_package='spack concretize --force && spack install && mz'

unalias mz_uc &> /dev/null
mz_uc() {
	echo "May work better in a new terminal!"
	spack env deactivate

	# Spack 1.1 selects a project by its top-level directory (the area) and keeps the selection per working directory,
	# so stay in the area for the whole sequence; older Spack selects by project name
	local startDirectory="$PWD"
	if ots_spack_is_1_1; then
		cd "$Base" && spack mpd select . 2>/dev/null; mpd_select_status=$?
	else
		spack mpd select tdaq-develop 2>/dev/null; mpd_select_status=$?
	fi
	if [ $mpd_select_status -eq 0 ]; then
		echo "tdaq-develop activated."
	else
		echo "Failed to activate tdaq-develop!"
		cd "$startDirectory"
		return 1
	fi

	spack mpd refresh --force && \
	spack env activate tdaq-develop && \
	mz
	local mzUcStatus=$?
	cd "$startDirectory"
	return $mzUcStatus
} #end mz_uc
export -f mz_uc
# alias mz_uc='(echo "must be in new terminal!" || spack env deactivate) || (spack mpd select tdaq-develop && spack mpd refresh && spack env activate tdaq-develop && mz)'

alias ots_diff="git --no-pager diff --ignore-all-space --color=always | less -R" #to see git changes without consuming console scroll space
alias ots_warn='UpdateOTS.sh --warn 1>/dev/null' #to see local git changes on demand
alias ots_warn_fast='UpdateOTS.sh --warnfast 1>/dev/null' #to see local git changes on demand

echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t  "
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t      setup_ots.sh creates some compiling aliases for you:"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t     ---------------"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t            mb                             ### for incremental build"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t            mz                             ### for clean build"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t            ml                             ### for incremental build into less for searchable errors, in order from the top"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t            mz_package                     ### for clean build after modifying spack packages"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t            mz_uc                          ### for clean build after updating adding/removing repos in srcs/ area"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t     ---------------"
echo -e "$(date +%d%h%y.%T) ots_compile_aliases.sh:${LINENO} |  \t  "
