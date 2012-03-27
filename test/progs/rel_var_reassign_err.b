type point {x int}

var p point;

fn test(t point) void
{
	p = (union p t);
	p = t;
}
