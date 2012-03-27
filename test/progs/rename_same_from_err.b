type point {x real, y real}

var p point;

fn cannot_rename() point
{
	return (rename x_coord = x, y_coord = x p);
}
