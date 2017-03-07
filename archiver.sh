#!/bin/sh
make veryclean
cd ..
tar -cf MailleAmaury.tar gnutella/*.h gnutella/*.c gnutella/Makefile gnutella/NOTES gnutella/README
