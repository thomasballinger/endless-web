// upstream 28bb9ed62
void AI::CircleAround(const Ship &ship, Command &command, const Body &target)
{
	Point direction = target.Position() - ship.Position();
	command.SetTurn(TurnToward(ship, direction));

	double length = direction.Length();
	if(length > 200. && ship.Facing().Unit().Dot(direction) >= 0.)
	{
		command |= Command::FORWARD;

		// If the ship is far away enough the ship should use the afterburner.
		if(length > 750. && ship.ShouldUseAfterburner())
			command |= Command::AFTERBURNER;
	}
}
