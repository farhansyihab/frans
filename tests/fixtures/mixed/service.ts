// ~/AgenAI/frans/tests/fixtures/mixed/service.ts
//
// Fixture Stage 10d — TypeScript interface + class.

export interface IService {
    run(): void;
}

export class Service implements IService {
    run(): void {
        console.log("running");
    }
}
