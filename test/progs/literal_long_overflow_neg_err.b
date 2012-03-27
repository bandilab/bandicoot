type T {v long}

var t T;

fn test() T
{
	return (select v > -9223372036854775809L t);
}
