#!/bin/zsh
cd -- "${0:A:h}" || exit 1
/usr/bin/python3 tools/play-native-mac.py "$@"
result=$?
if (( result != 0 )) && [[ -t 0 ]]; then
  printf '\nPress Return to close…'
  read -r
fi
exit "$result"
