// upstream 28bb9ed62
bool AI::MoveTo(const Ship &ship, Command &command, const Point &targetPosition,
	const Point &targetVelocity, double radius, double slow, double cruiseSpeed)
{
	const Point &position = ship.Position();
	const Point &velocity = ship.Velocity();
	const Angle &angle = ship.Facing();
	Point dp = targetPosition - position;
	Point dv = targetVelocity - velocity;

	double speed = dv.Length();

	bool isClose = (dp.Length() < radius);
	if(isClose && speed < slow)
		return true;

	bool shouldReverse = false;
	dp = targetPosition - StoppingPoint(ship, targetVelocity, shouldReverse);

	// Calculate target vector required to get where we want to be.
	Point tv = dp;
	bool hasCruiseSpeed = (cruiseSpeed > 0.);
	if(hasCruiseSpeed)
	{
		// The ship prefers a velocity at cruise-speed towards the target, so we need
		// to compare this preferred velocity to the current velocity and apply the
		// delta to get to the preferred velocity.
		tv = (dp.Unit() * cruiseSpeed) - velocity;
		// If we are moving close to our preferred velocity, then face towards the target.
		if(tv.LengthSquared() < .01)
			tv = dp;
	}

	bool isFacing = (tv.Unit().Dot(angle.Unit()) > .95);
	if(!isClose || (!isFacing && !shouldReverse))
		command.SetTurn(TurnToward(ship, tv));

	// Drag is not applied when not thrusting, so stop thrusting when close to max speed
	// to save energy. Work with a slightly lower maximum velocity to avoid border cases.
	// In order for a ship to use their afterburner, they must also have the forward
	// command active. Therefore, if this ship should use its afterburner, use the
	// max velocity with afterburner thrust included.
	double maxVelocity = ship.MaxVelocity(ship.ShouldUseAfterburner()) * .99;
	if(isFacing && (velocity.LengthSquared() <= maxVelocity * maxVelocity
			|| dp.Unit().Dot(velocity.Unit()) < .95))
	{
		// We set full forward power when we don't have a cruise-speed, when we are below
		// cruise-speed or when we need to do course corrections.
		bool movingTowardsTarget = (velocity.Unit().Dot(dp.Unit()) > .95);
		if(!hasCruiseSpeed || !movingTowardsTarget || velocity.Length() < cruiseSpeed)
			command |= Command::FORWARD;
	}
	else if(shouldReverse)
	{
		command.SetTurn(TurnToward(ship, velocity));
		command |= Command::BACK;
	}

	return false;
}
