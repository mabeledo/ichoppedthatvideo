#!/bin/sh
 
RETVAL="-retvalint -retvalother"
ARGS="+posixlib -likelybool +charint ${RETVAL}"
 
splint $ARGS $1
