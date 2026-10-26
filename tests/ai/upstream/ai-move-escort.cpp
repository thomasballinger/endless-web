// upstream 28bb9ed62
void AI::MoveEscort(Ship &ship, Command &command)
{
	const Ship &parent = *ship.GetParent();
	const System *currentSystem = ship.GetSystem();
	bool hasFuelCapacity = ship.MaxFuel();
	bool needsFuel = ship.NeedsFuel();
	bool isStaying = ship.GetPersonality().IsStaying() || !hasFuelCapacity;
	bool parentIsHere = (currentSystem == parent.GetSystem());
	// Check if the parent already landed, or has a target planet that is in the parent's system.
	const Planet *parentPlanet = (parent.GetPlanet() ? parent.GetPlanet() :
		(parent.GetTargetStellar() ? parent.GetTargetStellar()->GetPlanet() : nullptr));
	bool planetIsHere = (parentPlanet && parentPlanet->IsInSystem(parent.GetSystem()));
	bool systemHasFuel = hasFuelCapacity && currentSystem->HasFuelFor(ship);

	if(parent.Cloaking() == 1 && (ship.GetGovernment() != parent.GetGovernment()))
	{
		if(parent.GetGovernment() && parent.GetGovernment()->IsPlayer() &&
			ship.GetPersonality().IsEscort() && !ship.GetPersonality().IsUninterested())
		{
			// NPCs with the "escort" personality that are not uninterested
			// act as if they were escorts, following the cloaked flagship.
		}
		else
		{
			MoveIndependent(ship, command);
			return;
		}
	}

	// Non-staying escorts should route to their parent ship's system if not already in it.
	if(!parentIsHere && !isStaying)
	{
		if(ship.GetTargetStellar())
		{
			// An escort with an out-of-system parent only lands to
			// refuel or use a wormhole to route toward the parent.
			const Planet *targetPlanet = ship.GetTargetStellar()->GetPlanet();
			if(!targetPlanet || !targetPlanet->CanLand(ship)
					|| !ship.GetTargetStellar()->HasSprite()
					|| (!targetPlanet->IsWormhole() && ship.FuelFraction() == 1.))
				ship.SetTargetStellar(nullptr);
		}

		// If the ship has no destination or the destination is unreachable, route to the parent's system.
		if(!ship.GetTargetStellar() && (!ship.GetTargetSystem() || !ship.JumpNavigation().JumpFuel(ship.GetTargetSystem())))
		{
			// Route to the parent ship's system and check whether
			// the ship should land (refuel or wormhole) or jump.
			SelectRoute(ship, parent.GetSystem());
		}

		// Perform the action that this ship previously decided on.
		if(ship.GetTargetStellar())
		{
			MoveToPlanet(ship, command);
			command |= Command::LAND;
		}
		else if(ship.GetTargetSystem() && ship.JumpsRemaining())
		{
			PrepareForHyperspace(ship, command);
			command |= Command::JUMP;
			// If this ship is a parent to members of its fleet,
			// it should wait for them before jumping.
			if(!EscortsReadyToJump(ship))
				command |= Command::WAIT;
		}
		else if(systemHasFuel && ship.FuelFraction() < 1.)
			// Refuel so that when the parent returns, this ship is ready to rendezvous with it.
			Refuel(ship, command);
		else
			// This ship has no route to the parent's system, so park at the system's center.
			MoveTo(ship, command, Point(), Point(), 40., 0.1);
	}
	// If the parent is in-system and planning to jump, non-staying escorts should follow suit.
	else if(parent.Commands().Has(Command::JUMP) && parent.GetTargetSystem() && !isStaying)
	{
		if(parent.GetTargetSystem() != ship.GetTargetSystem())
			SelectRoute(ship, parent.GetTargetSystem());

		if(ship.GetTargetSystem())
		{
			PrepareForHyperspace(ship, command);
			command |= Command::JUMP;
			if(!(parent.IsEnteringHyperspace() || parent.IsReadyToJump()) || !EscortsReadyToJump(ship))
				command |= Command::WAIT;
		}
		else if(needsFuel && systemHasFuel)
			Refuel(ship, command);
		else if(ship.GetTargetStellar())
		{
			MoveToPlanet(ship, command);
			if(parent.IsEnteringHyperspace())
				command |= Command::LAND;
		}
		else if(needsFuel)
			// Return to the system center to maximize solar collection rate.
			MoveTo(ship, command, Point(), Point(), 40., 0.1);
		else
			// This ship has no route to the parent's destination system, so protect it until it jumps away.
			KeepStation(ship, command, parent);
	}
	// If an escort is out of fuel, they should refuel without waiting for the
	// "parent" to land (because the parent may not be planning on landing).
	else if(systemHasFuel && needsFuel)
		Refuel(ship, command);
	else if((parent.Commands().Has(Command::LAND) || parent.IsLanding()) && parentIsHere && planetIsHere)
	{
		if(parentPlanet->CanLand(ship))
		{
			ship.SetTargetSystem(nullptr);
			ship.SetTargetStellar(parent.GetTargetStellar());
			if(parent.IsLanding())
			{
				MoveToPlanet(ship, command);
				command |= Command::LAND;
			}
			else
				MoveWithParent(ship, command, parent);
		}
		else if(parentPlanet->IsWormhole())
		{
			const auto *wormhole = parentPlanet->GetWormhole();
			SelectRoute(ship, &wormhole->WormholeDestination(*currentSystem));

			if(ship.GetTargetSystem())
			{
				PrepareForHyperspace(ship, command);
				if(parent.IsLanding())
					command |= Command::JUMP;
			}
			else if(ship.GetTargetStellar())
			{
				MoveToPlanet(ship, command);
				if(parent.IsLanding())
					command |= Command::LAND;
			}
			else if(needsFuel)
				// Return to the system center to maximize solar collection rate.
				MoveTo(ship, command, Point(), Point(), 40., 0.1);
			else
				// This ship has no route to the parent's destination system, so protect it until it jumps away.
				MoveWithParent(ship, command, parent);
		}
		else
			MoveWithParent(ship, command, parent);
	}
	else if(parent.Commands().Has(Command::BOARD) && parent.GetTargetShip().get() == &ship)
		Stop(ship, command, .2);
	else
		MoveWithParent(ship, command, parent);
}
