type TaskId { taskId long }
type Queue { position int, TaskId }
type Task { TaskId, command string }
