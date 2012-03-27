type point {x real, y real}

var p point;

type point_res {x real}

fn cannot_project() point_res
{
	return (project x, x p);
}
