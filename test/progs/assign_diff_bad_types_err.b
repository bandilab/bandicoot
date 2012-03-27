type point {x real, y real}

type user {x int, y string}

var p point;

var u user;

fn cannot_assign() void
{
	p -= u;
}
