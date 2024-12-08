export class WowUpdaterService {
  constructor() {}
  checkIsInstalled(): Promise<boolean> {
    return new Promise((resolve) => {
      resolve(true);
    });
  }
  checkNeedsUpdates(): Promise<boolean> {
    return new Promise((resolve) => {
      resolve(false);
    });
  }
}

export const wowUpdaterService = new WowUpdaterService();
