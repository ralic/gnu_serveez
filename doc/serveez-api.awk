## Process this file with awk to produce a sed input.
#
# doc/serveez-api.awk
#
# Extract documentation string from source files and produce a sed
# script for further processing. 
#
# Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this package; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.  
#

# find start of C comment
/^\/\*/ \
{ 
  retry = 1
  while(retry) {
    # read lines until end of C comment has been reached
    end = index(substr($0, 4), "*/")
    doc = substr ($0, 4, index(substr($0, 4), "*/") - 1)
    while (end == 0) {
	if (getline <= 0) {
	    err = "unexpected EOF or error"
	    err = (err ": " ERRNO)
	    print err > "/dev/stderr"
	    exit
	}
	end = index($0, "*/")
	line = ""
	if (end == 0) { 
	    if (index($0, " *") == 1) {
		line = substr($0, 4)
	    }
	}
	# add a line to the documentation string
	if (length(line) > 0) { 
	    if (length(doc) > 0) { doc = (doc "\n") }
	    doc = (doc line)
	}
    }
    
    # parse the C function
    retry = 0
    found = 0
    dist = 0
    getline ret
    while (found == 0) {
	getline line
	nr = FNR
	# while trying to find a valid C function we found a new comment
	if (line ~ /^\/\*/) { 
	    $0 = line
	    retry = 1
	    break
	}
	if (line ~ /.+\(/) {
	    c_func = substr(line, 1, index(line, "(") - 1)
	    gsub(/[ \t]/, "", c_func)
	    line = substr(line, index(line, "(") + 1)

            # cleanup the comments in argument list
	    gsub(/\/\*[^\*]+\*\//, "", line)

	    # find out about the functions arguments
	    args = line
	    while (index(line, ")") == 0) {
		getline line
		gsub(/\/\*[^\*]+\*\//, "", line)
		args = (args line)
	    }
	    args = substr(args, 0, index(args, ")") - 1)
	    found++
	    break
	}
	dist++
	ret = line
    }

    # drop invalid functions
    if (retry) { continue }
    if (dist > 2 || c_func ~ /typedef/ || c_func ~ /struct/ ||
	c_func ~ /\#/ || !found || !length(c_func) || c_func ~ /\/\*/)
      { next }

    # cleanup tabs in argument list
    gsub(/\t/, " ", args)
    # seperate the arguments
    split(args, arg, ",")
    c_args = ""
    a = 0
    # count the arguments and save result
    for (x in arg) { a++ }
    for (x = 1; x <= a; x++) {
	# split the type definition
	split(arg[x], type, " ")
	# find last item
	i = 0
	for (v in type) { 
	    var = type[v]
	    i++
	}
	# check if last item has a '*' prefix
	if (index(var, "*")) {
	    type[i] = "*"
	    type[i+1] = substr(var, index(var, "*") + 1)
	    var = type[i+1]
	    i++
	}
	# rejoin the arguments
	c_arg = ""
	for (n = 1; n <= i; n++) {
	    if (type[n] != var) {
		c_arg = (c_arg type[n] " ")
	    }
	}
	c_arg = (c_arg "@var{" var "}")
	
	# rejoin the argument list
	if (c_args != "") { c_args = (c_args ", ") }
	c_args = (c_args c_arg)
    }

    # finally the texinfo function definition and documentation
    funcdef = ("@var{" ret "} " c_func " (" c_args ")")
    gsub(/\n/, "\\\n", doc)
    replace = ("@deftypefun " funcdef "\\\n" doc "\\\n" "@end deftypefun")
    sedexp = ("/" toupper(c_func) "_DEFUN/" " c\\\n" replace "\\\n")
    print sedexp
  }
}
