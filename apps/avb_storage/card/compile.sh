#!/bin/sh

if [[ -x "$JAVA_HOME" ]]; then exit 1; fi
if [[ -x "$JC_HOME" ]]; then exit 1; fi
export PATH=$PATH:$JC_HOME/bin

$JAVA_HOME/bin/javac -source 1.3 -target 1.1 -g -classpath $JC_HOME/lib/api.jar ./com/android/verifiedboot/bootstorage/*.java
