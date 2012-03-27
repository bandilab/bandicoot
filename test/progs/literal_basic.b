type T {i int, l long, r real, s string}

var t T;

fn test1() T
{
	return (select i < 2147483647 && i > -2147483648 && l < 9223372036854775807L && l > -9223372036854775808L t);
}

fn test2() T
{
	return (select i < -37 && i > +37 && i < i - -375 && i > 1 + +735 && i < i - 735 && i < i + 735 && r < -0.37 && r > +0.37 && s != "hello" t);
}
