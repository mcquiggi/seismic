# Makefile for the seismic decoder utility
# K. McQuiggin, 2016.10.01

decode: *.c	
	gcc decode.c -o decode
