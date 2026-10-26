// upstream 28bb9ed62
void AI::PickUp(const Ship &ship, Command &command, const Body &target)
{
	// Figure out the target's velocity relative to the ship.
	Point p = target.Position() - ship.Position();
	Point v = target.Velocity() - ship.Velocity();
	double vMax = ship.MaxVelocity();

	// Estimate where the target will be by the time we reach it.
	double time = RendezvousTime(p, v, vMax);
	if(std::isnan(time))
		time = p.Length() / vMax;
	double degreesToTurn = TO_DEG * acos(min(1., max(-1., p.Unit().Dot(ship.Facing().Unit()))));
	time += degreesToTurn / ship.TurnRate();
	p += v * time;

	// Move toward the target.
	command.SetTurn(TurnToward(ship, p));
	double dp = p.Unit().Dot(ship.Facing().Unit());
	if(dp > .7)
		command |= Command::FORWARD;

	// Use the afterburner if it will not cause you to miss your target.
	double squareDistance = p.LengthSquared();
	if(command.Has(Command::FORWARD) && ship.ShouldUseAfterburner())
		if(dp > max(.9, min(.9999, 1. - squareDistance / 10000000.)))
			command |= Command::AFTERBURNER;
}
