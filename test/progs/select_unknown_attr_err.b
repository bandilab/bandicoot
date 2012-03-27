type point {x int, y int}

fn test(p point) point
{
	return (select z > 10 p);
}
