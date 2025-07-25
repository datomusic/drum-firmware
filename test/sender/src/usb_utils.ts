import * as usb from 'usb';

export function findDevice(vid: number, pid: number): usb.Device | undefined {
  return usb.getDeviceList().find(d => d.deviceDescriptor.idVendor === vid && d.deviceDescriptor.idProduct === pid);
}

export async function pollForDevice(vid: number, pid:number, timeout: number = 10000): Promise<boolean> {
    const start = Date.now();
    while (Date.now() - start < timeout) {
        const device = findDevice(vid, pid);
        if (device) {
            return true;
        }
        await new Promise(resolve => setTimeout(resolve, 500));
    }
    return false;
}

export async function pollForDeviceDisappearance(vid: number, pid:number, timeout: number = 10000): Promise<boolean> {
    const start = Date.now();
    while (Date.now() - start < timeout) {
        const device = findDevice(vid, pid);
        if (!device) {
            return true;
        }
        await new Promise(resolve => setTimeout(resolve, 500));
    }
    return false;
}
