from pyreplib import replay
import sys, os

class Race:
    def __init__(self, name):
        self.name = name;
        self.states = {}

    def getState(self, buildings):
        buildings.sort()
        buildings = str(buildings)
        if buildings not in self.states:
            self.states[buildings] = len(self.states) + 1
        return self.states[buildings]

    def dump(self):
        if not os.path.exists(self.name[0]):
            os.makedirs(self.name[0])
        stats = open(self.name[0] + '/stats.csv', 'w')
        state_desc = [0] * max([0] + self.states.values())
        for key, value in self.states.items():
            state_desc[value - 1] = key
        i = 1
        for desc in state_desc:
            stats.write(str(i) + ',' + desc + '\r\n')
            i = i + 1
        stats.close()

races = {}
races['Zerg'] = Race('Zerg')
races['Terran'] = Race('Terran')
races['Protoss'] = Race('Protoss')

class Game:
    def __init__(self, fname):
        self.replay = replay.Replay(fname)
        if not(self.replay.is_valid()):
            raise Exception("invalid replay")
        self.parsePlayers()
        self.race = races[self.player.race_name]
        self.bucketSize = 300 # 1 frame = 42ms, 300 frames = 12.6s

    def parsePlayers(self):
        if len(self.replay.players) != 2:
            raise Exception("Expect 1on1 game")
        if self.replay.players[0].race == 2: # Protoss
            self.player = self.replay.players[1] # Analyze other player
        elif self.replay.players[1].race == 2:
            self.player = self.replay.players[0] # Analyze other player
        else:
            raise Exception("No Protoss player involved")
        
    def buildAction(self, building_type_id):
        if self.action_known():
            pass

    def bucketActions(self):
        self.buckets = {}
        for action in self.player.actions:
            if action.id == 0x0C: # Build action
                idx = action.tick % self.bucketSize
                bucket = self.buckets.get(idx, [])
                bucket.append(action)
                self.buckets[idx] = bucket

    def analyze(self):
        self.bucketActions()

        #print [self.player.race_name, self.replay.map_name]
        
        if not os.path.exists(self.player.race_name[0]):
            os.makedirs(self.player.race_name[0])
        data = open(self.player.race_name[0] + '/data.csv', 'w')
        
        buildings = []
        for step in range(max(self.buckets.keys())+1):
            bucket = self.buckets.get(step, [])
            for action in bucket:
                building = action.get_building_type()
                if building not in buildings:
                    buildings.append(building)
            data.write(str(self.race.getState(buildings)) + ',')
        data.write('\r\n')
        data.close()

# Expect name of replay file as first argument
fname = sys.argv[1]
try:
    game = Game(fname)
    game.analyze()
    for race in races:
        races[race].dump()
    
except Exception as e:
    print fname, ": bad replay", str(e)
