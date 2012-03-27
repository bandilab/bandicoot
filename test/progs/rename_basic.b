type point {x real, y real}

var p point;

type point_res {x_coord real, y_coord real}

fn s_rename() point_res
{
	return (rename x_coord = x, y_coord = y p);
}
