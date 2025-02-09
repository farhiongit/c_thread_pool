#!/bin/ksh
# Convert a header file into a markdown file.
# Lines ending with '\' are aggregated before parsing.
# A comment starting on the same line as a piece of code is related to this code and should be printed out BEFORE that code and followed by ':'.
# Code and comments are trimed of leading and trailing spaces.
# A comment ending with '.' is prined out followed by an empty line (md feature).
# Pieces of code are aggregated into code statements.
# A code statement is printed out preceded by '> ' (md feature).

# Text: concatenate a line ending with '\' with the next line.

# Pre-processor: lines starting with '#'.
# Pre-processor: "^#*(\s)define*\n".
# Pre-processor: "^#*(\s)include*\n".
# Pre-processor: ignore "^#*(\s)\n".
# Pre-processor: a comment is replaced by ' '.
# Pre-processor: trim leading and trailing spaces.

# Code: lines not starting with '#'.
# Code: a comment is replaced by ' '.
# Code: trim trailing spaces.
# Code: ignore empty lines.
# Code: prepend with '    '.

# Comment: "/* ... */".
# Comment: "// ...\n".
# Comment: trim leading and trailing spaces.
# Comment: replace trailing '.' by '.\n'.

typeset -i comment=0
typeset -i endcomment=0
typeset -i newpar=0
typeset -i verbatim=0
preline=
while IFS= read -r newline ; do
  preline="$preline${newline%"\\"}"
  if [[ "$newline" == *"\\" ]] ; then
    continue
  else
    line="$preline"
    preline=
  fi
  if (( comment )) && [[ "$line" == *(\s) ]] ; then
    print
    newpar=1
    verbatim=0
  fi
  if (( comment )) && (( newpar )) && [[ "$line" == +(\s)!(\s)* ]] ; then
    newpar=0
    verbatim=1
  fi
  while ! [[ "$line" == *(\s) ]] ; do
          if ! (( comment )) && [[ "$line" == *"/*"*"*/"* ]] ; then
            a="${line#*"/*"}"
            a="${a%%"*/"*}"
            #a="${a//"."+(\s)/".\n\n"}"
            a="${a/%"."*(\s)/".\n"}"
            print -- "\n${a#+(\s)}\n"
            code="$code ${line%%"/*"*}"
            line="${line#*"*/"}"
          elif ! (( comment )) && [[ "$line" == *"/*"* ]] ; then
            comment=1
            a="${line#*"/*"}"
            #a="${a//"."+(\s)/".\n\n"}"
            a="${a/%"."*(\s)/".\n"}"
            print -- "\n${a#+(\s)}"
            code="$code${line%%"/*"*}"
            line=""
          elif (( comment )) && [[ "$line" == *"*/"* ]] ; then
            comment=0
            a="${line%%"*/"*}"
            #a="${a//"."+(\s)/".\n\n"}"
            if ! (( verbatim )) ; then
                    a="${a/%"."*(\s)/".\n"}"
                    print -- "${a#+(\s)}\n"
            else
                    print -n -- "\t" ; print -r -- "${a%%+(\s)}"
            fi
            line="${line#*"*/"}"
            code="$code "
          elif (( comment )) ; then
            a="$line"
            #a="${a//"."+(\s)/".\n\n"}"
            if ! (( verbatim )) ; then
                    a="${a/%"."*(\s)/".\n"}"
                    print -- "${a#+(\s)}"
            else
                    print -n -- "\t" ; print -r -- "${a%%+(\s)}"
            fi
            line=""
          elif ! (( comment )) && [[ "$line" == *"//"* ]] ; then
            a="${line#*"//"}"
            #a="${a//"."+(\s)/".\n\n"}"
            a="${a/%"."*(\s)/".\n"}"
            print -- "\n${a#+(\s)}\n"
            code="$code${line%%"//"*}"
            line=""
          else
            code="$code$line"
            line=""
          fi
  done
  if [[ "$code" == "#"*(\s)"include"+(\s)* ]] ; then
    code="${code//"*"/"\\*"}"
    code="${code//"_"/"\\_"}"
    print -- "\n| Include |\n| - |\n|${code#"#"*(\s)"include"+(\s)} |\n"
  elif [[ "$code" == "#"*(\s)"define"+(\s)* ]] ; then
    code="${code//"*"/"\\*"}"
    code="${code//"_"/"\\_"}"
    code="${code#"#"*(\s)"define"+(\s)}"
    print -- "\n| Define | Value |\n| - | - |\n| ${code/+(\s)/" | "} |\n"
  elif [[ "$code" == "#"* ]] ; then
    ;
  elif [[ "$code" == *(\s)"typedef"+(\s)* ]] ; then
    code="${code//"*"/"\\*"}"
    code="${code//"_"/"\\_"}"
    code="${code#*(\s)"typedef"+(\s)}"
    code="${code%";"*(\s)}"
    print -- "\n| Type definition |\n| - |\n| $code |\n"
  elif [[ "$code" != *(\s) ]] ; then
    print -n -- "\t" ; print -r -- "$code"
  elif ! (( comment )) ; then
    print
  fi
  code=""
done
