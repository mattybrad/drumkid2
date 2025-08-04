import pcbnew

board = pcbnew.GetBoard()

for footprint in board.GetFootprints():
    value = footprint.GetValue()
    footprint.SetValue(value.upper())

pcbnew.Refresh()