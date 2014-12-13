from pyreplib import replay
from pyreplib.actions import LeaveGame
import sys, os, operator

# Collects information about all players of a certain race
class Race:
    def __init__(self, name):
        self.name = name;
        self.states = {}
        # Create P/T/Z directory
        if not os.path.exists('../hmm/' + self.name[0]):
            os.makedirs('../hmm/' + self.name[0])
        self.data = open('../hmm/' + self.name[0] + '/data.csv', 'w')
        self.replies = {}

    # Returns code for the set of buildings, using auto-increment to
    # create new codes for unseen building sets
    def getState(self, buildings):
        buildings.sort()
        buildings = str(buildings)
        if buildings not in self.states:
            self.states[buildings] = len(self.states) + 1
        return self.states[buildings]

    # Adds an observed reply state
    def reply(self, state, replyState, inc):
        if state not in self.replies:
            self.replies[state] = {}
        if replyState not in self.replies[state]:
            self.replies[state][replyState] = 0
        self.replies[state][replyState] += inc

    # Write one full game to data.csv
    def write(self, lst):
        first = True
        for i in lst:
            if first:
               first = False
            else:
               self.data.write(',')
            self.data.write(str(i))
        self.data.write('\n')

    def dumpStats(self):
        stats = open('../hmm/' + self.name[0] + '/stats.csv', 'w')
        state_desc = [0] * max([0] + self.states.values())
        for key, value in self.states.items():
            state_desc[value - 1] = key
        i = 1
        for desc in state_desc:
            stats.write(str(i) + ',' + desc + '\n')
            i = i + 1
        stats.close()

    def dumpReplies(self):
        reps = open('../hmm/' + self.name[0] + '/replies.csv', 'w')
        repsraw = open('../hmm/' + self.name[0] + '/replies-raw.csv', 'w')
        for key, value in self.replies.items():
            reply = len(self.states)
            if len(value) > 0:
                reply, _ = max(value.iteritems(), key=operator.itemgetter(1))
            reps.write(str(key) + ',' + str(reply) + '\n')
            repsraw.write(str(key) + '=' + str(value) + '\n')
        reps.close()
        repsraw.close()

    # Write the mapping from codes to building sets to stats.csv
    def dump(self):
        self.data.close()
        self.dumpStats()
        self.dumpReplies()

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
        self.player.race = races[self.player.race_name]
        self.oplayer.race = races[self.oplayer.race_name]
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
            'Shield Battery',
            'SCV',
            'Drone',
            'Overlord',
            'Probe']

    # Model the opponent of the protoss player (ignores TvZ games, etc.)
    def parsePlayers(self):
        if len(self.replay.players) != 2:
            raise Exception("Expect 1on1 game")
        if self.replay.players[0].race == 2: # Protoss
            self.player = self.replay.players[1] # Analyze other player
            self.oplayer = self.replay.players[0]
        elif self.replay.players[1].race == 2:
            self.player = self.replay.players[0] # Analyze other player
            self.oplayer = self.replay.players[1]
        else:
            raise Exception("No Protoss player involved")

    def getUnitType(self, action):
        # Build action
        if action.id == 0x0C:
            return (action.tick, action.get_building_type())
        # Train or Hatch action
        if action.id == 0x1F or action.id == 0x23:
            return (action.tick, action.get_unit_type())
        return (-1, 0)

    # Go through all actions in the game and put them in 12.6s buckets
    def bucketActions(self, player):
        buckets = {}
        for action in player.actions:
            tick, unittype = self.getUnitType(action)
            if tick >= 0 and unittype not in self.ignored:
                idx = tick / self.bucketSize
                bucket = buckets.get(idx, [])
                bucket.append(unittype)
                buckets[idx] = bucket
        return buckets

    def buildReply(self, player, oplayer, inc=1):
        prev = ""
        for i in range(max(len(player.trace), len(oplayer.trace))):
            state = player.trace[i] if i < len(player.trace) else player.trace[-1]
            ostate = oplayer.trace[i] if i < len(oplayer.trace) else oplayer.trace[-1]
            if prev != str(state) + "-" + str(ostate):
                player.race.reply(state, ostate, inc)
                prev = str(state) + "-" + str(ostate)

    def isLoser(self, player):
        return isinstance(player.actions[-1], LeaveGame)

    # Generate building set codes for every 12.6s of the game and write to file
    def analyzePlayer(self, player):
        buckets = self.bucketActions(player)
        buildings = []
        player.trace = []
        for step in range(max(buckets.keys())+1):
            bucket = buckets.get(step, [])
            for building in bucket:
                if building not in buildings:
                    buildings.append(building)
            player.trace.append(player.race.getState(buildings))

    def analyze(self):
        self.analyzePlayer(self.player)
        self.player.race.write(self.player.trace)
        if self.player.race.name == 'Protoss':  # Both players are Protoss
            self.analyzePlayer(self.oplayer)
            self.oplayer.race.write(self.oplayer.trace)
            if self.isLoser(self.player):   # Build reply from losing player
                self.buildReply(self.player, self.oplayer)
                self.buildReply(self.oplayer, self.player, -1)
            elif self.isLoser(self.oplayer):
                self.buildReply(self.oplayer, self.player)
                self.buildReply(self.player, self.oplayer, -1)
        elif self.isLoser(self.player): # Only my opponent is Protoss
            self.analyzePlayer(self.oplayer)
            self.buildReply(self.player, self.oplayer) # Build reply if I'm losing
        elif self.isLoser(self.oplayer):
            self.analyzePlayer(self.oplayer)
            self.buildReply(self.player, self.oplayer, -1)

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
                print fname, ": success"
            except Exception as e:
                print fname, ": bad replay", str(e)

# Write the accumulated building set mapping to stats.csv
for race in races:
    races[race].dump()
