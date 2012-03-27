type point {x real, y real}

fn rel_param(p point) void
{}

fn inline_rel_param1(p {x string}) void
{}

fn inline_rel_param2(p {x real, y real}) point
{
	return p;
}

fn prim_param(i int, r real, l long, s string) void
{}

fn param_comb_01(i int, r real, l long, s string, p point) void
{}

fn param_comb_02(i int, r real, p point, l long, s string) void
{}

fn param_comb_03(p point, i int, r real, l long, s string) void
{}

fn param_short_syntax(a b int, c d real, f long) void
{}
