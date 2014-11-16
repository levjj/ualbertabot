from pyreplib import replay
import sys, os

# Collects information about all players of a certain race
class Race:
    def __init__(self, name):
        self.name = name;
        self.states = {}
        # Create P/T/Z directory
        if not os.path.exists('../hmm/' + self.name[0]):
            os.makedirs('../hmm/' + self.name[0])
        self.data = open('../hmm/' + self.name[0] + '/data.csv', 'w')

    # Returns code for the set of buildings, using auto-increment to
    # create new codes for unseen building sets
    def getState(self, buildings):
        buildings.sort()
        buildings = str(buildings)
        if buildings not in self.states:
            self.states[buildings] = len(self.states) + 1
        return self.states[buildings]

    # Write one full game to data.csv
    def write(self, lst):
        for i in lst:
            self.data.write(str(i) + ',')
        self.data.write('\r\n')

    # Write the mapping from codes to building sets to stats.csv
    def dump(self):
        self.data.close()
        stats = open('../hmm/' + self.name[0] + '/stats.csv', 'w')
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

# Models information about a single game
class Game:
    def __init__(self, fname):
        self.replay = replay.Replay(fname)
        if not(self.replay.is_valid()):
            raise Exception("invalid replay")
        self.parsePlayers()
        self.race = races[self.player.race_name]
        self.bucketSize = 300 # 1 frame = 42ms, 300 frames = 12.6s
        self.ignored = [
            'Supply Depot',
            'Missile Turret',
            'Bunker',
            'Infested CC',
            'Creep Colony',
            'Spore Colony',
            'Sunken Colony',
            'Pylon',
            'Photon Cannon',
            'Shield Battery']

    # Model the opponent of the protoss player (ignores TvZ games, etc.)
    def parsePlayers(self):
        if len(self.replay.players) != 2:
            raise Exception("Expect 1on1 game")
        if self.replay.players[0].race == 2: # Protoss
            self.player = self.replay.players[1] # Analyze other player
        elif self.replay.players[1].race == 2:
            self.player = self.replay.players[0] # Analyze other player
        else:
            raise Exception("No Protoss player involved")

    # Go through all actions in the game and put them in 12.6s buckets
    def bucketActions(self):
        self.buckets = {}
        for action in self.player.actions:
            if action.id == 0x0C: # Build action
                idx = action.tick / self.bucketSize
                bucket = self.buckets.get(idx, [])
                bucket.append(action.get_building_type())
                self.buckets[idx] = bucket

    # Generate building set codes for every 12.6s of the game and write to file
    def analyze(self):
        self.bucketActions()
        #print [self.player.race_name, self.replay.map_name]

        buildings = []
        trace = []
        for step in range(max(self.buckets.keys())+1):
            bucket = self.buckets.get(step, [])
            for building in bucket:
                if building not in buildings and building not in self.ignored:
                    buildings.append(building)
            trace.append(self.race.getState(buildings))
        self.race.write(trace)

# Expect directory with replay files as first argument
rdir = sys.argv[1]

# Analyze all .rep files in directories below rdir
for root, dirs, files in os.walk(rdir):
    for f in files:
        if f.lower().endswith(".rep"):
            fname = os.path.join(root, f)
            try:
                game = Game(fname)
                game.analyze()
                # If both players are Protoss, do analysis for other player as well
                if game.race.name == 'Protoss':
                    game.player = game.replay.players[0]
                    game.analyze()

                print fname, ": success"
            except Exception as e:
                print fname, ": bad replay", str(e)

# Write the accumulated building set mapping to stats.csv
for race in races:
    races[race].dump()
