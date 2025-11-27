from typing import Callable, Dict, List


class BaseUI:
    def __init__(self, worker, render_table: Callable[[List[Dict[str, str]]], str]):
        self.worker = worker
        self.render_table = render_table

    def run(self) -> int:  # pragma: no cover
        raise NotImplementedError
