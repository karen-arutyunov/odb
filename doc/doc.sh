#! /usr/bin/env bash

version=2.5.0-b.6
date="$(date +"%B %Y")"

trap 'exit 1' ERR
set -o errtrace # Trap in functions.

function info () { echo "$*" 1>&2; }
function error () { info "$*"; exit 1; }

while [ $# -gt 0 ]; do
  case $1 in
    --clean)
      rm -f odb.xhtml odb.1
      rm -f odb-manual.ps odb-manual.pdf
      exit 0
      ;;
    *)
      error "unexpected $1"
      ;;
  esac
done

function compile () # <input-name> <output-name>
{
  local i=$1; shift
  local o=$1; shift

  # Use a bash array to handle empty arguments.
  #
  local ops=()
  while [ $# -gt 0 ]; do
    ops=("${ops[@]}" "$1")
    shift
  done

  # --html-suffix .xhtml
  cli -I .. -v project="odb" -v version="$version" -v date="$date" \
"${ops[@]}" --generate-html  --stdout \
--html-prologue-file odb-prologue.xhtml \
--html-epilogue-file odb-epilogue.xhtml \
"../odb/$i.cli" >"$o.xhtml"

  # --man-suffix .1
    cli -I .. -v project="odb" -v version="$version" -v date="$date" \
"${ops[@]}" --generate-man  --stdout \
--man-prologue-file odb-prologue.1 \
--man-epilogue-file odb-epilogue.1 \
"../odb/$i.cli" >"$o.1"
}

compile options odb --suppress-undocumented

# Manual.
#

#function compile_doc ()
#{
#  html2ps -f doc.html2ps:a4.html2ps -o "$n-a4.ps" "$n.xhtml"
#  ps2pdf14 -sPAPERSIZE=a4 -dOptimize=true -dEmbedAllFonts=true "$n-a4.ps" "$n-a4.pdf"
#
#  html2ps -f doc.html2ps:letter.html2ps -o "$n-letter.ps" "$n.xhtml"
#  ps2pdf14 -sPAPERSIZE=letter -dOptimize=true -dEmbedAllFonts=true "$n-letter.ps" "$n-letter.pdf"
#}

html2ps -f manual.html2ps -o odb-manual.ps manual.xhtml
ps2pdf14 -dOptimize=true -dEmbedAllFonts=true odb-manual.ps odb-manual.pdf
