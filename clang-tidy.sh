#!/bin/bash 

# This script executes clang-tidy on given folder(s) using regular expressions
# Use this command for individual files: 
#   clang-tidy -p "<BUILD_PATH>" -checks="<CHECKS>" "<SOURCE>" "<SOURCE>" ... 
# cgsg forever!

# print message before exit function
message() {
  if [ "$#" -ne 0 ]; then
    printf "$@\n" >&2;
  fi

  printf "Usage: $0 [-r REGEX_PATTERN] [-b BUILD_PATH] [-c CHECKS] [-C ADDITIONAL CHECKS] DIRECTORY [DIRECTORIES...]\n" >&2;
}

if [ "${#@}" -eq 0 ]; then
  message
  exit 1;
fi

# defaul variables values
REGEX_PATTERN='.*\.(h|hpp|hxx|c|cc|cpp|cxx)$'
BUILD_PATH="out"
CHECKS="-*,clang-analyzer-*,concurrency-*,cppcoreguidelines-*,-cppcoreguidelines-avoid-do-while,-cppcoreguidelines-avoid-goto,modernize-*,-modernize-use-trailing-return-type,perfomance-*,readability-*"
DIRECTORIES=""

# parse command arguments
while [ "$#" -gt 0 ]; do
  key="$1"

  case "$key" in
    -h|--help)
      message
      exit 0
      ;;
    -r)
      REGEX_PATTERN="$2"
      shift
      shift
      ;;
    -b)
      BUILD_PATH="$2"
      shift
      shift
      ;;
    -c)
      CHECKS="$2"
      shift
      shift
      ;;
    -C)
      CHECKS+=",$2"
      shift
      shift
      ;;
    *)
      DIRECTORIES+="$1 "
      shift
      ;;
  esac
done

echo "$DIRECTORIES -> $BUILD_PATH"
echo "Enabled checks: $CHECKS"

# obtain source files
if [ -z "$DIRECTORIES" ]; then
  message "No directories are provided."
  exit 1;
fi

CLANG_TIDY_FILES=$(
  find $DIRECTORIES -type f -regextype posix-extended -regex "$REGEX_PATTERN"
)

# execute clang-tidy
for FILE in ${CLANG_TIDY_FILES}; do
  echo "$FILE:"
  clang-tidy -p "$BUILD_PATH" -checks="$CHECKS" "$FILE"
done            	
