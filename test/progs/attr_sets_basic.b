type TaskId { taskId long }
type Queue { position int, TaskId }
type Task { TaskId, command string }

fn Test(x {Queue, name string}) TaskId {
    return project taskId, position, name x;
}
