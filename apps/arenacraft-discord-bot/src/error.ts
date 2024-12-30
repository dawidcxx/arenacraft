export class AppError extends Error {}
export class RetryError extends Error {}
export class RetryOverflowError extends Error {}
export class AlreadyRegistered extends AppError {}
export class AllRoomsTakenError extends AppError {
  constructor(public instanceId: number) {
    super("All rooms are taken unable to allocate game");
  }
}

export class UnableToFindCharacter extends AppError {
  constructor(public characterName: string) {
    super(`Unable to find character with name ${characterName}`);
  }
}
