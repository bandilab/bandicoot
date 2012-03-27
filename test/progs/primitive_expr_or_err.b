type test {i int, r real}

var t test;

fn s_or() test
{
	return (select r || i t);
}
