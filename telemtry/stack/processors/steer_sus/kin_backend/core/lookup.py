import numpy as np


class ShockToWheel:
    def __init__(self, npz_path):
        data = np.load(npz_path, allow_pickle=True)

        self.maps = {}
        for corner in ["FL", "FR", "RL", "RR"]:
            entry = data[corner].item()
            L = entry["L"]
            z = entry["z"]

            self.maps[corner] = (L, z)

    def z(self, corner, L_meas):
        L, z = self.maps[corner]

        if L_meas <= L[0]:
            return z[0]
        if L_meas >= L[-1]:
            return z[-1]

        return np.interp(L_meas, L, z)