#!/usr/bin/env python

import os
import sys
import posixpath

def log(pMsg):
	sys.stderr.write(pMsg + "\n")
	sys.stderr.flush()

def gen_paths_file(pPaths):
	tFileList = []
	
	for tPath in pPaths:
		for tRoot, tFolders, tFiles in os.walk(tPath):
			if os.path.basename(tRoot) != "test":	
				tFileNames = filter(lambda(s) : s.endswith(".h") or s.endswith(".cc"), tFiles)
				
				tRoot = tRoot.replace(os.path.sep, "/")

				for tFile in tFileNames:
					tFileList.append("\t\t\t'" +  posixpath.join(tRoot, tFile) + "',")

	tFileList.sort()
	
	print "{\n"
	print "\t'variables': {\n"
	print "\t\t'libcef_dll_sources': [\n"
	
	print "\n".join(tFileList)
	
	print "\t\t]\n\t}\n}\n"
	

#get include folder from the command line
tPaths = ["."]
if len(sys.argv) > 1:
	tPaths = sys.argv[1:]

gen_paths_file(tPaths)
