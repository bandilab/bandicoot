type point {x real, y real}

var temperature point;

var unemployment point;

var currency point;

fn get_pointless_results(p point) point
{
	return (union (minus (union temperature (join currency unemployment)) (join currency temperature)) p);
}

fn empty() void
{}
