from pyreplib import replay
import sys

# Expect name of replay file as first argument
fname = sys.argv[1]

class Game:
    def __init__(self, fname):
        self.replay = replay.Replay(fname)
        if not(self.replay.is_valid()):
            raise Exception("invalid replay")
        self.parsePlayers()

    def parsePlayers(self):
        if len(self.replay.players) != 2:
            raise Exception("Expect 1on1 game")
        if self.replay.players[0].race == 2: # Protoss
            self.player = self.replay.players[1] # Analyze other player
        elif self.replay.players[1].race == 2:
            self.player = self.replay.players[0] # Analyze other player
        else:
            raise Exception("No Protoss player involved")
        
    def analyze(self):
        resultVector = [self.player.race_name, self.replay.map_name]
        for action in self.player.actions:
            if action.id == 0x0C: # Build action
                building = action.get_building_type()
                resultVector.append((action.tick, building))
        return resultVector

try:
    game = Game(fname)
    buildtimes = game.analyze()
    print buildtimes
    
except Exception as e:
    print fname, ": bad replay", str(e)
