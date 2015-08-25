#!/usr/bin/python
import sys
import ccdash

if __name__ == "__main__":
	sys.argv[0] = "ccdash.py"
	sys.argv.insert(1, "scenario")
	rc = ccdash.main(sys.argv)
	sys.exit(rc)


