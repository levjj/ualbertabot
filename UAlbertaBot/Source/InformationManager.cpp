#include "Common.h"
#include "InformationManager.h"

#define SELF_INDEX 0
#define ENEMY_INDEX 1

// constructor
InformationManager::InformationManager() 
	: goForIt(false)
	, map(BWAPI::Broodwar)
	, lastFrameRegroup(false)
{
	initializeRegionInformation();

    current_enemy_state = predicted_enemy_state = 0;
    enemy_race = BWAPI::Broodwar->enemy()->getRace().getName().c_str()[0];
    if (enemy_race != 'U') // we know the race, so load the HMM data now
        loadHMMdata(enemy_race);
}

// get an instance of this
InformationManager & InformationManager::Instance() 
{
	static InformationManager instance;
	return instance;
}

std::map<string, string> readNamesFromStatsFile(string filename) {
    map<string, string> names;
    ifstream infile(filename.c_str());
    string line;
    if (infile.is_open()) {
        while (getline(infile, line)) {
            size_t start = line.find('[');
            size_t end = line.find(']');
            if (end == start + 1)
                continue;
            line = line.substr(start + 1, end - start - 1);
            do {
                start = line.find('\'');
                end = line.find('\'', start + 1);
                string name = line.substr(start + 1, end - start - 1);
                names[name] = name;
                line = line.substr(end + 1);
            } while (line.find(',') != string::npos);
        }
        infile.close();
    }
    return names;
}

void InformationManager::loadHMMdata(char race_c) {
    // load HMM
    string race = " ";
    race[0] = race_c;
    hmm.loadFromRace(race);

    // load states file
    string file = "?/stats.csv";
    file[0] = race_c;
    stats.readStatsFile(file);

    return;
    
    std::map<string, string> names = readNamesFromStatsFile(file);
    string filename = "?-names.txt";
    filename[0] = race_c;
    ofstream myfile;
    myfile.open(filename.c_str());
    for (std::map<std::string, std::string>::iterator iter = names.begin(); iter != names.end(); ++iter)
        myfile << iter->first << endl;
    myfile.close();
}

void InformationManager::updateHMM() {
    BWAPI::Broodwar->drawTextScreen(205, 344, "closest %d predicted %d", current_enemy_state, predicted_enemy_state); // update info on HUD

    char race_c = BWAPI::Broodwar->enemy()->getRace().getName().c_str()[0];
    if (race_c == 'U') // if we don't know race yet, return
        return;

    int frameCount = BWAPI::Broodwar->getFrameCount();
    if (enemy_race == 'U') { // Race changed from unknown to known, load all the data we need for that race
        enemy_race = race_c;
        loadHMMdata(enemy_race);
        for (int i = 0; i < frameCount / 300; ++i) // catch up observations, observe once for each 300 frames
            hmm.observe(1);
    }

    if (frameCount % 300) // only update observation and prediction every 300 frames (12.6s)
        return;

    set<string> target;

    BOOST_FOREACH(BWAPI::UnitType t, BWAPI::UnitTypes::allUnitTypes()) {
        int numUnits = enemyUnitData.getNumUnits(t);

        // if there exist units in the vector
        if (numUnits > 0) {
            string unit = t.getName();
            if (t.isBuilding()) {
                // Protoss buildings
                if (numUnits > 1 && unit.find("Nexus") != string::npos) target.insert("Nexus");
                if (unit.find("Assim") != string::npos)                 target.insert("Assimilator");
                if (unit.find("Gateway") != string::npos)               target.insert("Gateway");
                if (unit.find("Cyber") != string::npos)                 target.insert("Cybernetics Core");
                if (unit.find("Arbiter") != string::npos)               target.insert("Arbiter Tribunal");
                if (unit.find("Forge") != string::npos)                 target.insert("Forge");
                if (unit.find("Adun") != string::npos)                  target.insert("Citadel of Adun");
                if (unit.find("Fleet") != string::npos)                 target.insert("Fleet Beacon");
                if (unit.find("Robotics F") != string::npos)            target.insert("Robotics Facility");
                if (unit.find("Observ") != string::npos)                target.insert("Observatory");
                if (unit.find("Stargate") != string::npos)              target.insert("Stargate");
                if (unit.find("Templar") != string::npos)               target.insert("Templar Archives");
                if (unit.find("Support") != string::npos)               target.insert("Robotics Support Bay");
                if (unit.find("Photon Cannon") != string::npos)         target.insert("Forge"); // prereq
                // Terran buildings
                if (numUnits > 1 && unit.find("Comma") != string::npos) target.insert("Command Center");
                if (unit.find("Barracks") != string::npos)              target.insert("Barracks");
                if (unit.find("Academy") != string::npos)               target.insert("Academy");
                if (unit.find("Armory") != string::npos)                target.insert("Armory");
                if (unit.find("Refinery") != string::npos)              target.insert("Refinery");
                if (unit.find("Factory") != string::npos)               target.insert("Factory");
                if (unit.find("Engin") != string::npos)                 target.insert("Engineering Bay");
                if (unit.find("ComSat") != string::npos)                target.insert("ComSat");
                if (unit.find("Control") != string::npos)               target.insert("Control Tower");
                if (unit.find("Covert") != string::npos)                target.insert("Covert Ops");
                if (unit.find("Machine") != string::npos)               target.insert("Machine Shop");
                if (unit.find("Nuclear") != string::npos)               target.insert("Nuclear Silo");
                if (unit.find("Science") != string::npos)               target.insert("Science Facility");
                if (unit.find("Starport") != string::npos)              target.insert("Starport");
                if (unit.find("Phys") != string::npos)                  target.insert("Physics Lab");
                if (unit.find("Missile Turret") != string::npos)        target.insert("Engineering Bay"); //prereq
                if (unit.find("Bunker") != string::npos)                target.insert("Barracks"); //prereq
                // Zerg buildings
                if (numUnits > 1 && unit.find("Hatch") != string::npos) target.insert("Hatchery");
                if (unit.find("Spawning") != string::npos)              target.insert("Spawning Pool");
                if (unit.find("Spire") != string::npos)                 target.insert("Spire");
                if (unit.find("Evol") != string::npos)                  target.insert("Evolution Chamber");
                if (unit.find("Extractor") != string::npos)             target.insert("Extractor");
                if (unit.find("Hydra") != string::npos)                 target.insert("Hydralisk Den");
                if (unit.find("Queen") != string::npos)                 target.insert("Queens Nest");
                if (unit.find("Ultra") != string::npos)                 target.insert("Ultralisk Cavern");
                if (unit.find("Defiler") != string::npos)               target.insert("Defiler Mound");
                if (unit.find("Sunken") != string::npos)                target.insert("Spawning Pool"); // prereq
                if (unit.find("Spore") != string::npos)                 target.insert("Evolution Chamber"); // prereq
            } else if (t.canAttack()) { // units that can attacks, and prereqs for those units
                // Protoss units
                if (unit.find("Zealot") != string::npos) { target.insert("Zealot");  target.insert("Gateway"); }
                if (unit.find("Dragoon") != string::npos) { target.insert("Dragoon"); target.insert("Gateway"); target.insert("Cybernetics Core"); }
                if (unit.find("Dark Templar") != string::npos) { target.insert("Dark Templar"); target.insert("Gateway"); target.insert("Templar Archives"); }
                if (unit.find("Archon") != string::npos) { target.insert("High Templar"); target.insert("Templar Archives"); }
                if (unit.find("Reaver") != string::npos) { target.insert("Reaver"); target.insert("Robotics Facility"); target.insert("Robotics Support Bay"); }
                if (unit.find("Scout") != string::npos) { target.insert("Scout"); target.insert("Stargate"); }
                if (unit.find("Carrier") != string::npos) { target.insert("Carrier"); target.insert("Stargate"); target.insert("Fleet Beacon"); }
                if (unit.find("Arbiter") != string::npos) { target.insert("Arbiter"); target.insert("Stargate"); target.insert("Arbiter Tribunal"); }
                // Terran units
                if (unit.find("Firebat") != string::npos) { target.insert("Firebat"); target.insert("Barracks"); target.insert("Academy"); }
                if (unit.find("Ghost") != string::npos) { target.insert("Ghost"); target.insert("Barracks"); target.insert("Academy"); target.insert("Science Facility"); target.insert("Covert Ops"); }
                if (unit.find("Vulture") != string::npos) { target.insert("Vulture"); target.insert("Factory"); }
                if (unit.find("Siege Tank") != string::npos) { target.insert("Siege Tank"); target.insert("Factory"); target.insert("Machine Shop"); }
                if (unit.find("Goliath") != string::npos) { target.insert("Goliath"); target.insert("Factory"); target.insert("Armory"); }
                if (unit.find("Wraith") != string::npos) { target.insert("Wraith"); target.insert("Starport"); }
                if (unit.find("Battlecruiser") != string::npos) { target.insert("Battlecruiser"); target.insert("Starport"); target.insert("Control Tower"); target.insert("Science Facility"); target.insert("Physics Lab"); }
                if (unit.find("Marine") != string::npos) { target.insert("Marine"); target.insert("Barracks"); }
                if (unit.find("Nuke") != string::npos) { target.insert("Nuke"); target.insert("Factory"); target.insert("Nuclear Silo"); }
                if (unit.find("Valkyrie") != string::npos) { target.insert("Starport"); target.insert("Control Tower"); target.insert("Armory"); }
                // Zerg units
                if (unit.find("Zergling") != string::npos) { target.insert("Zergling"); target.insert("Spawning Pool"); }
                if (unit.find("Hydralisk") != string::npos) { target.insert("Hydralisk"); target.insert("Hydralisk Den"); }
                if (unit.find("Lurker") != string::npos) { target.insert("Lurker");  target.insert("Hydralisk"); target.insert("Hydralisk Den"); }
                if (unit.find("Mutalisk") != string::npos) { target.insert("Mutalisk"); target.insert("Spire"); }
                if (unit.find("Guardian") != string::npos) { target.insert("Guardian"); target.insert("Mutalisk"); target.insert("Spire"); }
                if (unit.find("Devourer") != string::npos) { target.insert("Devourer");  target.insert("Mutalisk"); target.insert("Spire"); }
                if (unit.find("Scourge") != string::npos) { target.insert("Scourge"); target.insert("Spire"); }
                if (unit.find("Ultralisk") != string::npos) { target.insert("Ultralisk"); target.insert("Ultralisk Cavern"); }
            } else if (t.isDetector()) {
                // Protoss units
                if (unit.find("Observer") != string::npos) { target.insert("Observer"); target.insert("Observatory"); }
                // Terran units
                if (unit.find("Science Vessel") != string::npos) { target.insert("Science Vessel"); target.insert("Starport"); target.insert("Science Facility"); }
                // Zerg units
            } else {
                // Protoss units
                if (unit.find("Dark Archon") != string::npos) { target.insert("Dark Templar"); target.insert("Templar Archives"); }
                if (unit.find("High Templar") != string::npos) { target.insert("High Templar"); target.insert("Gateway"); target.insert("Templar Archives"); }
                if (unit.find("Shuttle") != string::npos) { target.insert("Shuttle"); target.insert("Robotics Facility"); }
                // Terran units
                if (unit.find("Dropship") != string::npos) { target.insert("Dropship"); target.insert("Starport"); target.insert("Control Tower"); }
                if (unit.find("Medic") != string::npos) { target.insert("Medic"); target.insert("Barracks"); target.insert("Academy"); }
                // Zerg units
                if (unit.find("Queen") != string::npos) { target.insert("Queen"); target.insert("Queens Nest"); }
                if (unit.find("Defiler") != string::npos) { target.insert("Defiler"); target.insert("Defiler Mound"); }
                if (unit.find("Lurker Egg") != string::npos) { target.insert("Lurker");  target.insert("Hydralisk"); target.insert("Hydralisk Den"); }
                if (unit.find("Cocoon") != string::npos) { target.insert("Mutalisk"); target.insert("Spire"); }
            }
        }
    }
    unsigned int state = stats.getClosestState(target);
    hmm.observe(state);
    unsigned int predicted_state = hmm.predictMax(3); // predict enemy future state
    if (state != current_enemy_state || predicted_state != predicted_enemy_state) {
        current_enemy_state = state;
        predicted_enemy_state = predicted_state;
        BWAPI::Broodwar->printf("InformationManager: closest %d predicted %d", current_enemy_state, predicted_enemy_state);
    }
}

void InformationManager::update()
{
	updateUnitInfo();
	updateBaseLocationInfo();
	map.setUnitData(BWAPI::Broodwar);
	map.setBuildingData(BWAPI::Broodwar);

    updateHMM();
}

void InformationManager::updateUnitInfo() 
{
	// update enemy unit information
	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
	{
		updateUnit(unit);
	}

	// update enemy unit information
	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
	{
		updateUnit(unit);
	}

	// remove bad enemy units
	enemyUnitData.removeBadUnits();
	selfUnitData.removeBadUnits();
}

void InformationManager::initializeRegionInformation() 
{
	// set initial pointers to null
	mainBaseLocations[SELF_INDEX] = BWTA::getStartLocation(BWAPI::Broodwar->self());
	mainBaseLocations[ENEMY_INDEX] = BWTA::getStartLocation(BWAPI::Broodwar->enemy());

	// push that region into our occupied vector
	updateOccupiedRegions(BWTA::getRegion(mainBaseLocations[SELF_INDEX]->getTilePosition()), BWAPI::Broodwar->self());
}


void InformationManager::updateBaseLocationInfo() 
{
	occupiedRegions[SELF_INDEX].clear();
	occupiedRegions[ENEMY_INDEX].clear();
		
	// if we haven't found the enemy main base location yet
	if (!mainBaseLocations[ENEMY_INDEX]) 
	{ 
		// how many start locations have we explored
		int exploredStartLocations = 0;
		bool baseFound = false;

		// an undexplored base location holder
		BWTA::BaseLocation * unexplored = NULL;

		BOOST_FOREACH (BWTA::BaseLocation * startLocation, BWTA::getStartLocations()) 
		{
			if (isEnemyBuildingInRegion(BWTA::getRegion(startLocation->getTilePosition()))) 
			{
				baseFound = true;
				BWAPI::Broodwar->printf("Enemy base found by seeing it");
				mainBaseLocations[ENEMY_INDEX] = startLocation;
				updateOccupiedRegions(BWTA::getRegion(startLocation->getTilePosition()), BWAPI::Broodwar->enemy());
			}

			// if it's explored, increment
			if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) 
			{
				exploredStartLocations++;

			// otherwise set the unexplored base
			} 
			else 
			{
				unexplored = startLocation;
			}
		}

		// if we've explored every start location except one, it's the enemy
		if (!baseFound && exploredStartLocations == ((int)BWTA::getStartLocations().size() - 1)) 
		{
			BWAPI::Broodwar->printf("Enemy base found by process of elimination");
			mainBaseLocations[ENEMY_INDEX] = unexplored;
			updateOccupiedRegions(BWTA::getRegion(unexplored->getTilePosition()), BWAPI::Broodwar->enemy());
		}

	// otherwise we do know it, so push it back
	} 
	else 
	{
		updateOccupiedRegions(BWTA::getRegion(mainBaseLocations[ENEMY_INDEX]->getTilePosition()), BWAPI::Broodwar->enemy());
	}

	// for each enemy unit we know about
	FOR_EACH_UIMAP_CONST(iter, enemyUnitData.getUnits())
	{
		const UnitInfo & ui(iter->second);
		BWAPI::UnitType type = ui.type;

		// if the unit is a building
		if (type.isBuilding()) 
		{
			// update the enemy occupied regions
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), BWAPI::Broodwar->enemy());
		}
	}

	// for each of our units
	FOR_EACH_UIMAP_CONST(iter, selfUnitData.getUnits())
	{
		const UnitInfo & ui(iter->second);
		BWAPI::UnitType type = ui.type;

		// if the unit is a building
		if (type.isBuilding()) 
		{
			// update the enemy occupied regions
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), BWAPI::Broodwar->self());
		}
	}
}

void InformationManager::updateOccupiedRegions(BWTA::Region * region, BWAPI::Player * player) 
{
	// if the region is valid (flying buildings may be in NULL regions)
	if (region)
	{
		// add it to the list of occupied regions
		occupiedRegions[getIndex(player)].insert(region);
	}
}

int InformationManager::getIndex(BWAPI::Player * player)
{
	return player == BWAPI::Broodwar->self() ? SELF_INDEX : ENEMY_INDEX;
}

bool InformationManager::isEnemyBuildingInRegion(BWTA::Region * region) 
{
	// invalid regions aren't considered the same, but they will both be null
	if (!region)
	{
		return false;
	}

	FOR_EACH_UIMAP_CONST(iter, enemyUnitData.getUnits())
	{
		const UnitInfo & ui(iter->second);
		if (ui.type.isBuilding()) 
		{
			if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == region) 
			{
				return true;
			}
		}
	}

	return false;
}

int InformationManager::numEnemyUnitsInRegion(BWTA::Region * region) 
{
	// invalid region matching 
	if (!region)
	{
		return 0;
	}

	int unitsInRegion(0);
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits()) 
	{
		if (unit->isVisible() && BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == region) 
		{
			unitsInRegion++;
		}
	}

	return unitsInRegion;
}

const UIMap & InformationManager::getUnitInfo(BWAPI::Player * player) const
{
	return getUnitData(player).getUnits();
}

int InformationManager::numEnemyFlyingUnitsInRegion(BWTA::Region * region) 
{
	int unitsInRegion(0);
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits()) 
	{
		if (unit->isVisible() && BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == region && unit->getType().isFlyer()) 
		{
			unitsInRegion++;
		}
	}

	return unitsInRegion;
}

std::set<BWTA::Region *> & InformationManager::getOccupiedRegions(BWAPI::Player * player)
{
	return occupiedRegions[getIndex(player)];
}

BWTA::BaseLocation * InformationManager::getMainBaseLocation(BWAPI::Player * player) 
{
	return mainBaseLocations[getIndex(player)];
}

void InformationManager::drawUnitInformation(int x, int y) {
	
	std::string prefix = "\x04";

	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y-10, "\x03Lost:\x04 S \x1f%d \x07%d\x04 E \x1f%d \x07%d ", 
		selfUnitData.getMineralsLost(), selfUnitData.getGasLost(), enemyUnitData.getMineralsLost(), enemyUnitData.getGasLost());
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Enemy Unit Information: %s", BWAPI::Broodwar->enemy()->getRace().getName().c_str());
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04UNIT NAME");
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+140, y+20, "\x04#");
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+160, y+20, "\x04X");

	int yspace = 0;

	// for each unit in the queue
	BOOST_FOREACH (BWAPI::UnitType t, BWAPI::UnitTypes::allUnitTypes()) 
	{
		int numUnits = enemyUnitData.getNumUnits(t);
		int numDeadUnits = enemyUnitData.getNumDeadUnits(t);

		// if there exist units in the vector
		if (numUnits > 0) 
		{
			if (t.isDetector())			{ prefix = "\x10"; }		
			else if (t.canAttack())		{ prefix = "\x08"; }		
			else if (t.isBuilding())	{ prefix = "\x03"; }
			else						{ prefix = "\x04"; }

			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "%s%s", prefix.c_str(), t.getName().c_str());
			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+140, y+40+((yspace)*10), "%s%d", prefix.c_str(), numUnits);
			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+160, y+40+((yspace++)*10), "%s%d", prefix.c_str(), numDeadUnits);
		}
	}
}

void InformationManager::onStart() {}

void InformationManager::updateUnit(BWAPI::Unit * unit)
{
	if (unit->getPlayer() == BWAPI::Broodwar->enemy())
	{
		enemyUnitData.updateUnit(unit);
	}
	else if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		selfUnitData.updateUnit(unit);
	}
}

// is the unit valid?
bool InformationManager::isValidUnit(BWAPI::Unit * unit) 
{
	// we only care about our units and enemy units
	if (unit->getPlayer() != BWAPI::Broodwar->self() && unit->getPlayer() != BWAPI::Broodwar->enemy()) 
	{
		return false;
	}

	// if it's a weird unit, don't bother
	if (unit->getType() == BWAPI::UnitTypes::None || unit->getType() == BWAPI::UnitTypes::Unknown ||
		unit->getType() == BWAPI::UnitTypes::Zerg_Larva || unit->getType() == BWAPI::UnitTypes::Zerg_Egg) 
	{
		return false;
	}

	// if the position isn't valid throw it out
	if (!unit->getPosition().isValid()) 
	{
		return false;	
	}

	// s'all good baby baby
	return true;
}

void InformationManager::onUnitDestroy(BWAPI::Unit * unit) 
{ 
	// erase the unit
	if (unit->getPlayer() == BWAPI::Broodwar->enemy())
	{
		enemyUnitData.removeUnit(unit);
	}
	else if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		selfUnitData.removeUnit(unit);
	}
}

BWAPI::Unit * InformationManager::getClosestUnitToTarget(BWAPI::UnitType type, BWAPI::Position target)
{
	BWAPI::Unit * closestUnit = NULL;
	double closestDist = 100000;

	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == type)
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

bool InformationManager::isCombatUnit(BWAPI::UnitType type) const
{
	if (type == BWAPI::UnitTypes::Zerg_Lurker || type == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		return false;
	}

	// no workers or buildings allowed
	if (type.isWorker())
	{
		return false;
	}

	// check for various types of combat units
	if (type.canAttack() || type == BWAPI::UnitTypes::Terran_Medic)
	{
		return true;
	}
		
	return false;
}

void InformationManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, BWAPI::Position p, BWAPI::Player * player, int radius) 
{
	// for each unit we know about for that player
	FOR_EACH_UIMAP_CONST(iter, getUnitData(player).getUnits())
	{
		const UnitInfo & ui(iter->second);

		// if it's a combat unit we care about
		if (isCombatUnit(ui.type))
		{
			// determine its attack range
			int range = 0;
			if (ui.type.groundWeapon() != BWAPI::WeaponTypes::None)
			{
				range = ui.type.groundWeapon().maxRange() + 40;
			}

			// if it can attack into the radius we care about
			if (ui.lastPosition.getDistance(p) <= (radius + range))
			{
				// add it to the vector
				unitInfo.push_back(ui);
			}
		}
	}
}

bool InformationManager::nearbyForceHasCloaked(BWAPI::Position p, BWAPI::Player * player, int radius) 
{
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(p.x(), p.y(), radius, BWAPI::Colors::Red);

	FOR_EACH_UIMAP_CONST(iter, getUnitData(player).getUnits())
	{
		const UnitInfo & ui(iter->second);
		BWAPI::UnitType type(ui.type);

		// we don't care about workers
		if (type.isWorker())
		{
			continue;
		}

		int range = 0;
		if (type.groundWeapon() != BWAPI::WeaponTypes::None)
		{
			range = type.groundWeapon().maxRange() + 40;
		}

		// if it's outside the radius we don't care
		if (ui.lastPosition.getDistance(p) > (radius + range))
		{
			continue;
		}

		if (type == BWAPI::UnitTypes::Zerg_Lurker ||
				type == BWAPI::UnitTypes::Protoss_Dark_Templar ||
				type == BWAPI::UnitTypes::Terran_Wraith)
		{
			return true;
		}
	}

	return false;
}

int InformationManager::getNumUnits(BWAPI::UnitType t, BWAPI::Player * player)
{
	return getUnitData(player).getNumUnits(t);
}

const UnitData & InformationManager::getUnitData(BWAPI::Player * player) const
{
	return (player == BWAPI::Broodwar->self()) ? selfUnitData : enemyUnitData;
}

const UnitData & InformationManager::getUnitData(BWAPI::Unit * unit) const
{
	return getUnitData(unit->getPlayer());
}

bool InformationManager::enemyHasCloakedUnits()
{
	return enemyUnitData.hasCloakedUnits();
}

bool InformationManager::enemyHasDetector()
{
	return enemyUnitData.hasDetectorUnits();
}

bool InformationManager::tileContainsUnit(BWAPI::TilePosition tile)
{
	return map.canBuildHere(tile);
}