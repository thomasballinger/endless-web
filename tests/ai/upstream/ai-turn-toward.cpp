// upstream 28bb9ed62
double AI::TurnToward(const Ship &ship, const Point &vector, const double precision)
{
	Point facing = ship.Facing().Unit();
	double cross = vector.Cross(facing);

	double dot = vector.Dot(facing);
	if(dot > 0.)
	{
		// Is the facing direction aligned with the target direction with sufficient precision?
		// The maximum angle between the two directions is given by: arccos(sqrt(precision)).
		bool close = false;
		if(precision < 1. && precision > 0. && dot * dot >= precision * vector.LengthSquared())
			close = true;
		double angle = asin(min(1., max(-1., cross / vector.Length()))) * TO_DEG;
		// Is the angle between the facing and target direction smaller than
		// the angle the ship can turn through in one step?
		if(fabs(angle) < ship.TurnRate())
		{
			// If the ship is within one step of the target direction,
			// and the facing is already sufficiently aligned with the target direction,
			// don't turn any further.
			if(close)
				return 0.;
			return -angle / ship.TurnRate();
		}
	}

	bool left = cross < 0.;
	return left - !left;
}
