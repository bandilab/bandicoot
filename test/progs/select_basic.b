type point {x int, y int}

fn test(p point) point
{
	return (select x > 10 p);
}

var p point;

fn test2(i int) point
{
	return (select x > i p);
}
