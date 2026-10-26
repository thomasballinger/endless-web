// upstream 28bb9ed62
void AI::Step(Command &activeCommands)
{
	// First, figure out the comparative strengths of the present governments.
	const System *playerSystem = player.GetSystem();
	map<const Government *, int64_t> strength;
	UpdateStrengths(strength, playerSystem);
	CacheShipLists();

	// Update the counts of how long ships have been outside the "invisible fence."
	// If a ship ceases to exist, this also ensures that it will be removed from
	// the fence count map after a few seconds.
	for(auto it = fenceCount.begin(); it != fenceCount.end(); )
	{
		it->second -= FENCE_DECAY;
		if(it->second < 0)
			it = fenceCount.erase(it);
		else
			++it;
	}
	for(const auto &it : ships)
	{
		const System *system = it->GetActualSystem();
		if(system && it->Position().Length() >= system->InvisibleFenceRadius())
		{
			int &value = fenceCount[&*it];
			value = min(FENCE_MAX, value + FENCE_DECAY + 1);
		}
	}

	// Allow all formation-positioners to handle their internal administration to
	// prepare for the next cycle.
	for(auto &bodyIt : formations)
		for(auto &positionerIt : bodyIt.second)
			positionerIt.second.Step();

	const Ship *flagship = player.Flagship();
	// Step increments from 0 to 62. Using a bitwise and instead of a modulus for the better performance it provides,
	// even if it's minor. This means that things occur slightly off of exactly once per second, but the difference in
	// behavior between the two methods is negligible.
	step = (step + 1) & 63;
	// Targeting occurs about twice per second, so the target step only goes from 0 to 30.
	int targetStep = step & 31;
	int targetTurn = 0;
	// Scatter recalculations occur about once per second, so it can compare against the full step counter.
	int scatterTurn = 0;
	int minerCount = 0;
	const int maxMinerCount = minables.empty() ? 0 : 9;
	bool opportunisticEscorts = !Preferences::Has("Turrets focus fire");
	bool fightersRetreat = Preferences::Has("Damaged fighters retreat");
	const int npcMaxMiningTime = GameData::GetGamerules().NPCMaxMiningTime();
	for(const auto &it : ships)
	{
		// A destroyed ship can't do anything.
		if(it->IsDestroyed())
			continue;
		// Skip any carried fighters or drones that are somehow in the list.
		if(!it->GetSystem())
			continue;

		if(it.get() == flagship)
		{
			// Player cannot do anything if the flagship is landing.
			if(!flagship->IsLanding())
				MovePlayer(*it, activeCommands);
			continue;
		}

		const Government *gov = it->GetGovernment();
		const Personality &personality = it->GetPersonality();
		double healthRemaining = it->HealthFraction();
		bool isPresent = (it->GetSystem() == playerSystem);
		bool isStranded = IsStranded(*it);
		bool thisIsLaunching = (isPresent && HasDeployments(*it));
		if(isStranded || it->IsDisabled())
		{
			// Attempt to find a friendly ship to render assistance.
			if(!it->GetPersonality().IsDerelict())
				AskForHelp(*it, isStranded, flagship);

			if(it->IsDisabled())
			{
				// Ships other than escorts should deploy fighters if disabled.
				if(!it->IsYours() || thisIsLaunching)
				{
					it->SetCommands(Command::DEPLOY);
					Deploy(*it, !(it->IsYours() && fightersRetreat));
				}
				// Avoid jettisoning cargo as soon as this ship is repaired.
				if(personality.IsAppeasing())
				{
					double health = .5 * it->ShieldFraction() + it->HullFraction();
					double &threshold = appeasementThreshold[it.get()];
					threshold = max((1. - health) + .1, threshold);
				}
				continue;
			}
		}
		// Overheated ships are effectively disabled, and cannot fire, cloak, etc.
		if(it->IsOverheated())
			continue;

		Command command;
		firingCommands.SetHardpoints(it->Weapons().size(), it->Decorations().size());
		if(it->IsYours())
		{
			if(it->HasBays() && thisIsLaunching)
			{
				// If this is a carrier, launch whichever of its fighters are at
				// good enough health to survive a fight.
				command |= Command::DEPLOY;
				Deploy(*it, !fightersRetreat);
			}
			if(player.IsCloaking())
				command |= Command::CLOAK;
		}

		// Cloak if the AI considers it appropriate.
		if(!it->IsYours() || !player.IsCloaking())
			if(DoCloak(*it, command))
			{
				// The ship chose to retreat from its target, e.g. to repair.
				it->SetCommands(command);
				continue;
			}

		shared_ptr<Ship> parent = it->GetParent();
		if(parent && parent->IsDestroyed())
		{
			// An NPC that loses its fleet leader should attempt to
			// follow that leader's parent. For most mission NPCs,
			// this is the player. Any regular NPCs and mission NPCs
			// with "personality uninterested" become independent.
			parent = parent->GetParent();
			it->SetParent(parent);
		}

		// Pick a target and automatically fire weapons.
		shared_ptr<Ship> target = it->GetTargetShip();
		shared_ptr<Minable> targetAsteroid = it->GetTargetAsteroid();
		shared_ptr<Flotsam> targetFlotsam = it->GetTargetFlotsam();
		if(isPresent && it->IsYours() && targetFlotsam && FollowOrders(*it, command))
			continue;
		// Determine if this ship was trying to scan its previous target. If so, keep
		// track of this ship's scan time and count.
		if(isPresent && !it->IsYours())
		{
			// Assume that if the target is friendly, not disabled, of a different
			// government to this ship, and this ship has scanning capabilities
			// then it was attempting to scan the target. This isn't a perfect
			// assumption, but should be good enough for now.
			bool cargoScan = it->CargoScanPower();
			bool outfitScan = it->OutfitScanPower();
			if((cargoScan || outfitScan) && target && !target->IsDisabled()
				&& !target->GetGovernment()->IsEnemy(gov) && target->GetGovernment() != gov)
			{
				++scanTime[&*it];
				if(it->CargoScanFraction() >= 1.)
					cargoScans[&*it].insert(&*target);
				if(it->OutfitScanFraction() >= 1.)
					outfitScans[&*it].insert(&*target);
			}
		}
		if(isPresent && !personality.IsSwarming())
		{
			// Each ship only switches targets about twice a second, so that it can
			// focus on damaging one particular ship.
			targetTurn = (targetTurn + 1) & 31;
			if(targetTurn == targetStep || !target || target->IsDestroyed() || (target->IsDisabled() &&
					(personality.Disables() || (!FighterHitHelper::IsValidTarget(target.get()) && !personality.IsVindictive())))
					|| (target->IsFleeing() && personality.IsMerciful()) || !target->IsTargetable())
			{
				target = FindTarget(*it);
				it->SetTargetShip(target);
			}
		}
		if(isPresent)
		{
			AimTurrets(*it, firingCommands, it->IsYours() ? opportunisticEscorts : personality.IsOpportunistic());
			if(targetAsteroid)
				AutoFire(*it, firingCommands, *targetAsteroid);
			else
				AutoFire(*it, firingCommands);
		}

		// If this ship is hyperspacing, or in the act of
		// launching or landing, it can't do anything else.
		if(it->IsHyperspacing() || it->Zoom() < 1.)
		{
			it->SetCommands(command);
			it->SetCommands(firingCommands);
			continue;
		}

		// Run away if your hostile target is not disabled
		// and you are either badly damaged or have run out of ammo.
		// Player ships never stop targeting hostiles, while hostile mission NPCs will
		// do so only if they are allowed to leave.
		const bool shouldFlee = ShouldFlee(*it);
		if(!it->IsYours() && shouldFlee && target && target->GetGovernment()->IsEnemy(gov) && !target->IsDisabled()
			&& (!it->GetParent() || !it->GetParent()->GetGovernment()->IsEnemy(gov)))
		{
			// Make sure the ship has somewhere to flee to.
			const System *system = it->GetSystem();
			if(it->JumpsRemaining() && (!system->Links().empty() || it->JumpNavigation().HasJumpDrive()))
				target.reset();
			else
				for(const StellarObject &object : system->Objects())
					if(object.HasSprite() && object.HasValidPlanet() && object.GetPlanet()->IsInhabited()
							&& object.GetPlanet()->CanLand(*it))
					{
						target.reset();
						break;
					}

			if(target)
				// This ship has nowhere to flee to: Stop fleeing.
				it->SetFleeing(false);
			else
			{
				// This ship has somewhere to flee to: Remove target and mark this ship as fleeing.
				it->SetTargetShip(target);
				it->SetFleeing();
			}
		}
		else if(it->IsFleeing())
			it->SetFleeing(false);

		// Special actions when a ship is heavily damaged:
		if(healthRemaining < RETREAT_HEALTH + .25)
		{
			// Cowards abandon their fleets.
			if(parent && personality.IsCoward())
			{
				parent.reset();
				it->SetParent(parent);
			}
			// Appeasing ships jettison cargo to distract their pursuers.
			if(personality.IsAppeasing() && it->Cargo().Used())
				DoAppeasing(it, &appeasementThreshold[it.get()]);
		}

		// If recruited to assist a ship, follow through on the commitment
		// instead of ignoring it due to other personality traits.
		shared_ptr<Ship> shipToAssist = it->GetShipToAssist();
		if(shipToAssist)
		{
			if(shipToAssist->IsDestroyed() || shipToAssist->GetSystem() != it->GetSystem()
					|| shipToAssist->IsLanding() || shipToAssist->IsHyperspacing()
					|| shipToAssist->GetGovernment()->IsEnemy(gov)
					|| (!shipToAssist->IsDisabled() && !shipToAssist->NeedsFuel() && !shipToAssist->NeedsEnergy()))
			{
				if(target == shipToAssist)
				{
					target.reset();
					it->SetTargetShip(nullptr);
				}
				shipToAssist.reset();
				it->SetShipToAssist(weak_ptr<Ship>());
			}
			else if(!it->IsBoarding())
			{
				MoveTo(*it, command, shipToAssist->Position(), shipToAssist->Velocity(), 40., .8);
				command |= Command::BOARD;
			}

			if(shipToAssist)
			{
				it->SetTargetShip(shipToAssist);
				it->SetCommands(command);
				it->SetCommands(firingCommands);
				continue;
			}
		}

		// This ship may have updated its target ship.
		double targetDistance = numeric_limits<double>::infinity();
		target = it->GetTargetShip();
		if(target)
			targetDistance = target->Position().Distance(it->Position());

		// Special case: if the player's flagship tries to board a ship to
		// refuel it, that escort should hold position for boarding.
		isStranded |= (flagship && it == flagship->GetTargetShip() && CanBoard(*flagship, *it)
			&& autoPilot.Has(Command::BOARD));

		// Stranded ships that have a helper need to stop and be assisted.
		bool strandedWithHelper = isStranded &&
			(HasHelper(*it, NeedsFuel(*it), NeedsEnergy(*it)) || it->GetPersonality().IsDerelict() || it->IsYours());

		// Behave in accordance with personality traits.
		if(isPresent && personality.IsSwarming() && !strandedWithHelper)
		{
			// Swarming ships should not wait for (or be waited for by) any ship.
			if(parent)
			{
				parent.reset();
				it->SetParent(parent);
			}
			// Flock between allied, in-system ships.
			DoSwarming(*it, command, target);
			it->SetCommands(command);
			it->SetCommands(firingCommands);
			continue;
		}

		if(isPresent && personality.IsSecretive())
		{
			if(DoSecretive(*it, command))
			{
				it->SetCommands(command);
				continue;
			}
		}

		// Surveillance NPCs with enforcement authority (or those from
		// missions) should perform scans and surveys of the system.
		if(isPresent && personality.IsSurveillance() && !strandedWithHelper
				&& (scanPermissions[gov] || it->IsSpecial()))
		{
			DoSurveillance(*it, command, target);
			it->SetCommands(command);
			it->SetCommands(firingCommands);
			continue;
		}

		// Ships that harvest flotsam prioritize it over stopping to be refueled.
		if(isPresent && personality.Harvests() && DoHarvesting(*it, command))
		{
			it->SetCommands(command);
			it->SetCommands(firingCommands);
			continue;
		}

		// Attacking a hostile ship, fleeing and stopping to be refueled are more important than mining.
		if(isPresent && personality.IsMining() && !shouldFlee && !target && !strandedWithHelper && maxMinerCount)
		{
			// Miners with free cargo space and available mining time should mine. Mission NPCs
			// should mine even if there are other miners or they have been mining a while.
			if(it->Cargo().Free() >= 5 && IsArmed(*it) && (it->IsSpecial()
					|| (++miningTime[&*it] < npcMaxMiningTime && ++minerCount < maxMinerCount)))
			{
				if(it->HasBays())
				{
					command |= Command::DEPLOY;
					Deploy(*it, false);
				}
				DoMining(*it, command);
				it->SetCommands(command);
				it->SetCommands(firingCommands);
				continue;
			}
			// Fighters and drones should assist their parent's mining operation if they cannot
			// carry ore, and the asteroid is near enough that the parent can harvest the ore.
			if(it->CanBeCarried() && parent && miningTime[parent.get()] < 3601)
			{
				const shared_ptr<Minable> &minable = parent->GetTargetAsteroid();
				if(minable && minable->Position().Distance(parent->Position()) < 600.)
				{
					it->SetTargetAsteroid(minable);
					MoveToAttack(*it, command, *minable, firingCommands);
					AutoFire(*it, firingCommands, *minable);
					it->SetCommands(command);
					it->SetCommands(firingCommands);
					continue;
				}
			}
			it->SetTargetAsteroid(nullptr);
		}

		// Handle carried ships:
		if(it->CanBeCarried())
		{
			// A carried ship must belong to the same government as its parent to dock with it.
			bool hasParent = parent && !parent->IsDestroyed() && parent->GetGovernment() == gov;
			bool inParentSystem = hasParent && parent->GetSystem() == it->GetSystem();
			// NPCs may take 30 seconds or longer to find a new parent. Player
			// owned fighter shouldn't take more than a few seconds.
			bool findNewParent = it->IsYours() ? !Random::Int(30) : !Random::Int(1800);
			bool parentHasSpace = inParentSystem && parent->BaysFree(it->Attributes().Category());
			if(findNewParent && parentHasSpace && it->IsYours())
				parentHasSpace = parent->CanCarry(*it);
			if(!hasParent || (!inParentSystem && !it->JumpNavigation().JumpFuel()) || (!parentHasSpace && findNewParent))
			{
				// Find the possible parents for orphaned fighters and drones.
				auto parentChoices = vector<shared_ptr<Ship>>{};
				parentChoices.reserve(ships.size() * .1);
				auto getParentFrom = [&it, &gov, &parentChoices](const list<shared_ptr<Ship>> &otherShips) -> shared_ptr<Ship>
				{
					// Fighters with the staying personality should only dock with carriers that are also staying.
					bool isStaying = it->GetPersonality().IsStaying();
					for(const auto &other : otherShips)
						if(other->GetGovernment() == gov && other->GetSystem() == it->GetSystem()
							&& (!isStaying || other->GetPersonality().IsStaying()) && !other->CanBeCarried())
						{
							if(!other->IsDisabled() && other->CanCarry(*it))
								return other;
							else
								parentChoices.emplace_back(other);
						}
					return shared_ptr<Ship>();
				};
				// Mission ships should only pick amongst ships from the same mission.
				auto missionIt = it->IsSpecial() && !it->IsYours()
					? find_if(player.Missions().begin(), player.Missions().end(),
						[&it](const Mission &m) { return m.HasShip(it); })
					: player.Missions().end();

				shared_ptr<Ship> newParent;
				if(missionIt != player.Missions().end())
				{
					auto &npcs = missionIt->NPCs();
					for(const auto &npc : npcs)
					{
						// Don't reparent to NPC ships that have not been spawned.
						if(!npc.ShouldSpawn())
							continue;
						newParent = getParentFrom(npc.Ships());
						if(newParent)
							break;
					}
				}
				else
					newParent = getParentFrom(ships);

				// If a new parent was found, then this carried ship should always reparent
				// as a ship of its own government is in-system and has space to carry it.
				if(newParent)
					parent = newParent;
				// Otherwise, if one or more in-system ships of the same government were found,
				// this carried ship should flock with one of them, even if they can't carry it.
				else if(!parentChoices.empty())
					parent = parentChoices[Random::Int(parentChoices.size())];
				// Player-owned carriables that can't be carried and have no ships to flock with
				// should keep their current parent, or if it is destroyed, their parent's parent.
				else if(it->IsYours())
				{
					if(parent && parent->IsDestroyed())
						parent = parent->GetParent();
				}
				// All remaining non-player ships should forget their previous parent entirely.
				else
					parent.reset();

				// Player-owned carriables should defer to player carrier if
				// selected parent can't carry it. This is necessary to prevent
				// fighters from jumping around fleet when there's not enough
				// bays.
				if(it->IsYours() && parent && parent->GetParent() && !parent->CanCarry(*it))
					parent = parent->GetParent();

				if(it->GetParent() != parent)
					it->SetParent(parent);
			}
			// Otherwise, check if this ship wants to return to its parent (e.g. to repair).
			else if(parentHasSpace && ShouldDock(*it, *parent, playerSystem))
			{
				it->SetTargetShip(parent);
				MoveTo(*it, command, parent->Position(), parent->Velocity(), 40., .8);
				command |= Command::BOARD;
				it->SetCommands(command);
				it->SetCommands(firingCommands);
				continue;
			}
			// If we get here, it means that the ship has not decided to return
			// to its mothership. So, it should continue to be deployed.
			command |= Command::DEPLOY;
		}
		// If this ship has decided to recall all of its fighters because combat has ceased,
		// it comes to a stop to facilitate their reboarding process.
		bool mustRecall = false;
		if(!target && it->HasBays() && !(it->IsYours() ?
				thisIsLaunching : it->Commands().Has(Command::DEPLOY)))
			for(const weak_ptr<Ship> &ptr : it->GetEscorts())
			{
				shared_ptr<const Ship> escort = ptr.lock();
				// Note: HasDeployOrder is always `false` for NPC ships, as it is solely used for player ships.
				if(escort && escort->CanBeCarried() && !escort->HasDeployOrder() && escort->GetSystem() == it->GetSystem()
						&& !escort->IsDisabled() && it->BaysFree(escort->Attributes().Category()))
				{
					mustRecall = true;
					break;
				}
			}

		// Construct movement / navigation commands as appropriate for the ship.
		if(mustRecall || (strandedWithHelper && !it->GetPersonality().IsDerelict()))
		{
			// Stopping to let fighters board or to be refueled takes priority
			// even over following orders from the player.
			if(it->Velocity().Length() > VELOCITY_ZERO || !target)
				Stop(*it, command);
			else
			{
				command.SetTurn(TurnToward(*it, TargetAim(*it, firingCommands)));
				it->SetVelocity({0., 0.});
			}
		}
		else if(FollowOrders(*it, command))
		{
			// If this is an escort and it followed orders, its only final task
			// is to convert completed MOVE_TO orders into HOLD_POSITION orders.
			UpdateOrders(*it);
		}
		// Hostile "escorts" (i.e. NPCs that are trailing you) only revert to
		// escort behavior when in a different system from you. Otherwise,
		// the behavior depends on what the parent is doing, whether there
		// are hostile targets nearby, and whether the escort has any
		// immediate needs (like refueling).
		else if(!parent)
			MoveIndependent(*it, command);
		else if(parent->GetSystem() != it->GetSystem())
		{
			if(personality.IsStaying() || !it->MaxFuel())
				MoveIndependent(*it, command);
			else
				MoveEscort(*it, command);
		}
		// From here down, we're only dealing with ships that have a "parent"
		// which is in the same system as them.
		else if(parent->GetGovernment()->IsEnemy(gov))
		{
			// Fight your target, if you have one.
			if(target)
				MoveIndependent(*it, command);
			// Otherwise try to find and fight your parent. If your parent
			// can't be both targeted and pursued, then don't follow them.
			else if(parent->IsTargetable() && CanPursue(*it, *parent))
				MoveEscort(*it, command);
			else
				MoveIndependent(*it, command);
		}
		else if(parent->IsDisabled() && !it->CanBeCarried())
		{
			// Your parent is disabled, and is in this system. If you have enemy
			// targets present, fight them. Otherwise, repair your parent.
			if(target)
				MoveIndependent(*it, command);
			else if(!parent->GetPersonality().IsDerelict())
				it->SetShipToAssist(parent);
			else
				CircleAround(*it, command, *parent);
		}
		else if(personality.IsStaying())
			MoveIndependent(*it, command);
		// This is a friendly escort. If the parent is getting ready to
		// jump, always follow.
		else if(parent->Commands().Has(Command::JUMP) && !strandedWithHelper)
			MoveEscort(*it, command);
		// Timid ships always stay near their parent. Injured player
		// escorts will stay nearby until they have repaired a bit.
		else if((personality.IsTimid() || (it->IsYours() && healthRemaining < RETREAT_HEALTH))
				&& parent->Position().Distance(it->Position()) > 500.)
			MoveEscort(*it, command);
		// Otherwise, attack targets depending on your hunting attribute.
		else if(target && (targetDistance < 2000. || personality.IsHunting()))
			MoveIndependent(*it, command);
		// This ship does not feel like fighting.
		else
			MoveEscort(*it, command);

		// Force ships that are overlapping each other to "scatter".
		// About once per second they check which ships they are close to and might need to scatter away from,
		// as ships that were close to each other recently are likely to still be close to each other now.
		scatterTurn = (scatterTurn + 1) & 63;
		DoScatter(*it, command, scatterTurn == step);

		it->SetCommands(command);
		it->SetCommands(firingCommands);
	}
}
