#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv) > 1 else False)

print("+----------------------------------------------------------------------+")
print("| AMC: Scripted Trade Types (Barrier, Accumulator, KnockOutSwap, etc.) |")
print("+----------------------------------------------------------------------+")
oreex.print_headline("Run ORE AMC exposure for scripted trade types")
oreex.run("Input/ore_scripted_amc_eq.xml")

oreex.setup_plot("SP5 European Option")
oreex.plot("scripted_trades_eq/exposure_trade_01:EuropeanEquityCallOptionBlackScholes.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_01:EuropeanEquityCallOptionBlackScholes.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - European Option(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")

oreex.setup_plot("SP5 Equity Forward")
oreex.plot("scripted_trades_eq/exposure_trade_02:EuropeanEquityForwardBlackScholes.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_02:EuropeanEquityForwardBlackScholes.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - Equity Forward(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")

oreex.setup_plot("SP5 European Option Scripted (MC)")
oreex.plot("scripted_trades_eq/exposure_trade_03:EuropeanEquityCallOptionScriptedMC.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_03:EuropeanEquityCallOptionScriptedMC.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - European Option(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")

oreex.setup_plot("SP5 Equity Forward(MC)")
oreex.plot("scripted_trades_eq/exposure_trade_04:EuropeanEquityForwardScriptedMC.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_04:EuropeanEquityForwardScriptedMC.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - Equity Forward(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")

oreex.setup_plot("SP5 American Option")
oreex.plot("scripted_trades_eq/exposure_trade_05:AmericanEquityCallOptionBlackScholes.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_05:AmericanEquityCallOptionBlackScholes.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - Equity Option(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")


oreex.setup_plot("SP5 American Option(AMC)")
oreex.plot("scripted_trades_eq/exposure_trade_06:AmericanEquityOptionScriptedAMC.csv", 2, 3, 'b', "EPE", linestyle='-')
oreex.plot("scripted_trades_eq/exposure_trade_06:AmericanEquityOptionScriptedAMC.csv", 2, 4, 'r', "ENE", linestyle='-')
oreex.decorate_plot(title="Exposure - Equity Option(SP5, Strike=2000)")
oreex.save_plot_to_file(subdir="Output/scripted_trades_eq")
