# Makefile for easier commands

.PHONY: default

default: help ;

build-deb:
	@equivs-build package.conf

help:
	@echo "Poor Man's Catalog"
	@echo "build-deb  Build the debian package"
