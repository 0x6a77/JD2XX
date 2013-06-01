#!/bin/bash
java -Xcheck:jni -Djava.library.path=".." -cp "../jd2xx.jar:." TestListener
