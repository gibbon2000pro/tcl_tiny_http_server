#! /bin/bash

cat script.tcl | curl -v -X POST --data-binary @- "http://localhost:8889/"