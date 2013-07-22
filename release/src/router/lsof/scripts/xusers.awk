#!/usr/bin/awk -f
################################################################
#  
#  Program Name  :  xusers
#  Date Created  :  02-27-97
#  Author        :  Dan A. Mercer
#  Email         :  damercer@mmm.com
#                :
#  Description   : Print list of users and applications signed on
#                : X workstations
################################################################
# standard help message
function help(hlpmsg) {
basename = ARGV[0]
sub(/.*\//,"",basename)
printf "Format:  %s [o=[hi]] [s=cdlp] [pattern]\n", basename
print  "Print list of users and applications signed on X workstations"
print  "NOTE: applicationname is truncated to 9 chars"
print  "Arguments:"
print  "           o=[h|i]      - Options"
print  "              h         - help - print this message"
print  "              i         - case insensitive pattern search"
print  "           s=[c|d|l|p]  - Sort Options"
print  "              c         - sort by command"
print  "              d         - sort by display name"
print  "              l         - sort by login name"
print  "              p         - sort by pid"
print  "           pattern      - regex pattern to search commands against"

if (length(hlpmsg)) print hlpmsg
exit
}
BEGIN {
# process command line
for (i=1;i<ARGC;i++) {
   if (ARGV[i] ~ /^o=/) {
      if (options)
         help("duplicate option string")
      options = ARGV[i]
      sub(/^o=/,"",options)
      if (options !~ /^[hi]$/)
         help("Invalid options " options)
      if ("h" == options)
         help("")
      else
         igncase = 1
      }
   else if (ARGV[i] ~ /^s=/) {
      if (sortorder)
         help("duplicate sort order string")
      sortorder = ARGV[i]
      sub(/^s=/,"",sortorder)
      if (sortorder !~ /^[cdlp]$/)
         help("Invalid sort order: '" sortorder "'")
      if ("p" == sortorder) {
         sort = "sort -kn2"
         }
      else if ("c" == sortorder) {
         # the 'b' option means ignore leading blanks
         sort = "sort -kb3"
         }
      else if ("l" == sortorder) {
         sort = "sort -kb1"
         }
      else {
         sort = "sort -kb4"
         }
      }
   else {
      if (pattern)
         help("duplicate pattern string")
      pattern = ARGV[i]
      }
   }

# default is to sort by pid
sort = (sort) ? sort : "sort -kn2"

# check for igncase
if (pattern && igncase)
   pattern = tolower(pattern)

# set default pattern
pattern = (pattern) ? pattern : ".*"

cmd = "lsof -FpLcn  -awP -iTCP:6000"
#            ||||| ||||  |
#            ||||| ||||  X servers use port 6000
#            ||||| |||don't list port names
#            ||||| ||suppress warning messages
#            ||||| |and all conditions
#            ||||| |options
#            |||||
#            ||||Internet addresses
#            |||command name
#            ||login name
#            |process id
#            Format string
# Output consists of one record per pid,  followed by newline
# delimited fields for command, Login name, and network address
# The pid is preceded by a 'p',  command by a 'c',
# Login name by an L, and network connection by an 'n'.  There may
# be multiple 'n' entries (for instance for vuewm)

while ((cmd | getline field) > 0) {
   type = substr(field,1,1)
   sub("^.","",field)
   if ("p" == type) {
      # always output first
      pid = field
      PID[pid] = ++ct
      }
   else if ("c" == type) {
      # always output second
      XAPPL[pid] = field
      }
   else if ("L" == type) {
      # always output fourth
      USER[pid] = field
      }
   else if ("n" == type) {
      # may be multiple instances - we just use the last
      gsub(".*->|:6000","",field)
      DPY[pid] = field
      }
   }
close(cmd)

printf "%8s  %5s  %-9s  %s\n","USER","PID","COMMAND","DISPLAY"
for (pid in PID) {
   if (((igncase) ? tolower(XAPPL[pid]) : XAPPL[pid]) ~ pattern)
      printf "%8s  %5d  %-9s  %s\n", USER[pid],pid,XAPPL[pid],DPY[pid] | sort
   }

close(sort)
exit
}
