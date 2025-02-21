import cv2
import cv2.typing as cvt
import numpy.typing as npt
import onnxruntime as ort
from utils import image_transforms as transforms

class Infer():
    def __init__(self, model_path: str) -> None:
        self.model_path: str = model_path
        self.frame: cvt.MatLike = None
        self.frame_clean: cvt.MatLike = None
        ort.disable_telemetry_events()
        opts = ort.SessionOptions()
        opts.inter_op_num_threads = 1
        opts.intra_op_num_threads = 2
        opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        opts.add_session_config_entry("session.intra_op.allow_spinning", "0")  # ~3% savings worth ~6ms avg latency. Not noticeable at 60fps?
        opts.enable_mem_pattern = False
        self.sess = ort.InferenceSession(
            f"{model_path}/onnx/model.onnx",
            opts,
            #providers=["DmlExecutionProvider", "CPUExecutionProvider"],
            providers=["CPUExecutionProvider"],
        )
        self.iname = self.sess.get_inputs()[0].name
        self.oname = self.sess.get_outputs()[0].name

    def _preprocess_frame(self, frame: cvt.MatLike) -> npt.NDArray:
        self.frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        self.frame = transforms.to_tensor(self.frame)
        self.frame = transforms.unsqueeze(self.frame, 0)
        return self.frame

    def run(self, input: cvt.MatLike) -> list[float]:
        input = self._preprocess_frame(frame = input)
        out = self.sess.run([self.oname], {self.iname: input})
        out = out[0][0]
        return out

