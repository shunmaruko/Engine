#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.run("Input/ore_scripted_amc.xml")
oreex.get_times("Output/amc/log.txt")
