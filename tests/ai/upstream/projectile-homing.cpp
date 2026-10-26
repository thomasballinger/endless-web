// upstream 28bb9ed62
		if(homing)
		{
			// Vector d is the direction we want to turn towards.
			Point d;
			bool isFacingAway = false;
			if(currentTarget)
			{
				d = currentTarget->Position() - position;
				isFacingAway = d.Dot(angle.Unit()) < 0.;
			}

			// The very dumbest of homing missiles lose their target if pointed
			// away from it.
			if(isFacingAway && weapon->HasBlindspot())
				target.reset();
			else if(currentTarget && hasLock)
			{
				Point unit = d.Unit();
				double drag = weapon->Drag();
				double trueVelocity = drag ? accel / drag : velocity.Length();
				double stepsToReach = d.Length() / trueVelocity;
				// At the highest homing level, compensate for target motion.
				if(weapon->Leading())
				{
					if(unit.Dot(currentTarget->Velocity()) < 0.)
					{
						// If the target is moving toward this projectile, the intercept
						// course is where the target and the projectile have the same
						// velocity normal to the distance between them.
						Point normal(unit.Y(), -unit.X());
						double vN = normal.Dot(currentTarget->Velocity());
						double vT = sqrt(max(0., trueVelocity * trueVelocity - vN * vN));
						d = vT * unit + vN * normal;
					}
					else
					{
						// Adjust the target's position based on where it will be when we
						// reach it (assuming we're pointed right towards it).
						d += stepsToReach * currentTarget->Velocity();
						stepsToReach = d.Length() / trueVelocity;
					}
					unit = d.Unit();
				}

				double cross = angle.Unit().Cross(unit);

				double desiredTurn = TO_DEG * asin(cross);
				if(fabs(desiredTurn) > turn)
					turn = copysign(turn, desiredTurn);
				else
					turn = desiredTurn;

				// Levels 3 and 4 stop accelerating when facing away.
				if(weapon->ThrottleControl())
				{
					double stepsToFace = desiredTurn / turn;

					// If you are facing away from the target, stop accelerating.
					if(stepsToFace * 1.5 > stepsToReach)
						accel = 0.;
				}
			}
			// Turn in a random direction if this weapon is confused.
			else if(isConfused)
				turn *= confusionDirection;
			// If a weapon is homing but has no target, do not turn it.
			else
				turn = 0.;
		}
