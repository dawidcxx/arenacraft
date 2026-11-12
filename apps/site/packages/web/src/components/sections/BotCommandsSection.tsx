import { SectionIntro } from "@/components/SectionIntro";

const COMMANDS = [
  { command: "/register", description: "create a new in game account or reset credentials" },
  {
    command: "/setpassword",
    description: "use it to set your own password instead of using the auto generated one",
  },
  { command: "/toprankers", description: "get a table of the current top 10" },
  { command: "/ping", description: "check if the bot is alive" },
] as const;

export function BotCommandsSection() {
  return (
    <section className="grid border-b-2 border-divider md:grid-cols-2">
      <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
        <SectionIntro index="05 · Bot" title="Bot commands" />
      </div>

      <div className="p-4">
        <table className="w-full border-collapse text-left text-sm">
          <thead>
            <tr>
              <th
                scope="col"
                className="w-[117px] border-b-2 border-divider p-2 text-[11px] font-bold tracking-[0.08em] text-ink/60 uppercase"
              >
                Command
              </th>
              <th
                scope="col"
                className="border-b-2 border-divider p-2 text-[11px] font-bold tracking-[0.08em] text-ink/60 uppercase"
              >
                What it does
              </th>
            </tr>
          </thead>
          <tbody>
            {COMMANDS.map(({ command, description }) => (
              <tr key={command}>
                <td className="border-b border-divider p-2 font-mono text-sm">{command}</td>
                <td className="border-b border-divider p-2">{description}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}